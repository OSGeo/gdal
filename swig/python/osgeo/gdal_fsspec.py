# SPDX-License-Identifier: MIT
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>

"""Module exposing GDAL Virtual File Systems (VSI) as fsspec implementations.

   Importing "osgeo.gdal_fsspec" requires the Python "fsspec"
   (https://filesystem-spec.readthedocs.io/en/latest/) module to be available.

   A generic "vsi" fsspec protocol is available. All GDAL VSI file names must be
   simply prefixed with "vsi://". For example:

   - "vsi://data/byte.tif" to access relative file "data/byte.tif"
   - "vsi:///home/user/byte.tif" to access absolute file "/home/user/byte.tif"
   - "vsi:///vsimem/byte.tif" (note the 3 slashes) to access VSIMem file "/vsimem/byte.tif"

   Each VSI file system is also registered as a distinct fsspec protocol, such
   as "vsimem", "vsicurl", "vsizip", "vsitar", etc.

   Examples:

   - "vsimem://byte.tif" to access file "/vsimem/byte.tif"
   - "vsicurl://http://example.com/foo" to access file "/vsicurl/http://example.com/foo"
   - "vsis3://my_bucket/byte.tif" to access file "/vsis3/my_bucket/byte.tif"
   - "vsizip:///home/user/my.zip/foo.tif" (note the 3 slashes to indicate absolute path)
     to access (absolute) file "/vsizip//home/user/my.zip/foo.tif"
   - "vsizip://my.zip/foo.tif" to access (relative) file "/vsizip/my.zip/foo.tif"

   :since: GDAL 3.11
"""

from pathlib import PurePath

from fsspec.registry import register_implementation
from fsspec.spec import AbstractFileSystem
from fsspec.utils import stringify_path

from osgeo import gdal


class VSIFileSystem(AbstractFileSystem):
    """Implementation of AbstractFileSystem for a GDAL Virtual File System"""

    @classmethod
    def _get_gdal_path(cls, path):
        """Return a GDAL compatible file from a fsspec file name.

        For the file system using the generic "vsi" protocol,
        remove the leading vsi:// if found (normally, it should be there,
        but most AbstractFileSystem implementations seem to be ready to remove
        it if found)

        For specialized file systems, like vsimem://, etc., for an input
        like "vsimem:///foo", return "/vsimem/foo". And for an input like
        "/foo" also return "/vsimem/foo".
        """

        if isinstance(path, PurePath):
            path = stringify_path(path)

        if cls.protocol == "vsi":
            # "vsi://something" just becomes "something"
            if path.startswith("vsi://"):
                return path[len("vsi://") :]

            return path

        else:
            list_protocols_that_need_leeding_slash = [
                "vsis3",
                "vsigs",
                "vsiaz",
                "vsioss",
                "vsiswift",
            ]
            list_protocols_that_need_leeding_slash += [
                item + "_streaming" for item in list_protocols_that_need_leeding_slash
            ]
            list_protocols_that_need_leeding_slash.append("vsimem")

            # Deal with paths like "vsis3://foo"
            full_protocol = cls.protocol + "://"
            if path.startswith(full_protocol):
                path = path[len(full_protocol) :]

            # Deal with paths like "/foo" with a VSIFileSystem that is something like "vsis3"
            if (
                cls.protocol in list_protocols_that_need_leeding_slash
                and not path.startswith("/")
            ):
                path = "/" + path

            return "/" + cls.protocol + path

    def _open(
        self,
        path,
        mode="rb",
        block_size=None,
        autocommit=True,
        cache_options=None,
        **kwargs,
    ):
        """Implements AbstractFileSystem._open()"""

        path = self._get_gdal_path(path)
        return gdal.VSIFile(path, mode)

    def info(self, path, **kwargs):
        """Implements AbstractFileSystem.info()"""

        gdal_path = self._get_gdal_path(path)
        stat = gdal.VSIStatL(gdal_path)
        if stat is None:
            raise FileNotFoundError(path)
        if stat.IsDirectory():
            ret = {
                "name": self._strip_protocol(path),
                "size": 0,
                "type": "directory",
            }
        else:
            ret = {
                "name": self._strip_protocol(path),
                "size": stat.size,
                "type": "file",
                "mtime": stat.mtime,
            }
        if stat.mode:
            ret["mode"] = stat.mode
        return ret

    def modified(self, path):
        """Implements AbstractFileSystem.modified()"""

        gdal_path = self._get_gdal_path(path)
        stat = gdal.VSIStatL(gdal_path)
        if stat is None:
            raise FileNotFoundError(path)
        import datetime

        return datetime.datetime.fromtimestamp(stat.mtime)

    def ls(self, path, detail=True, **kwargs):
        """Implements AbstractFileSystem.ls()"""

        fs_path = self._strip_protocol(path)
        gdal_path = self._get_gdal_path(path)
        ret = []
        directory = gdal.OpenDir(gdal_path)
        if directory is None:
            stat = gdal.VSIStatL(gdal_path)
            if stat is None:
                raise FileNotFoundError(path)
            return [fs_path]

        try:
            while True:
                entry = gdal.GetNextDirEntry(directory)
                if entry is None:
                    break

                ret_entry = {
                    "name": fs_path + "/" + entry.name,
                    "type": (
                        "file"
                        if (entry.mode & 32768) != 0
                        else "directory"
                        if (entry.mode & 16384) != 0
                        else None
                    ),
                }
                if ret_entry["type"] == "file":
                    ret_entry["size"] = entry.size if entry.sizeKnown else None
                if entry.mtimeKnown:
                    ret_entry["mtime"] = entry.mtime
                ret.append(ret_entry)
        finally:
            gdal.CloseDir(directory)
        return ret

    def mkdir(self, path, create_parents=True, **kwargs):
        """Implements AbstractFileSystem.mkdir()"""

        # Base fs.makedirs() may call us with "/vsimem"
        if (
            path + "/" in gdal.GetFileSystemsPrefixes()
            or path in gdal.GetFileSystemsPrefixes()
        ):
            return

        gdal_path = self._get_gdal_path(path)
        if gdal.VSIStatL(gdal_path):
            raise FileExistsError(path)
        if create_parents:
            ret = gdal.MkdirRecursive(gdal_path, 0o755)
        else:
            ret = gdal.Mkdir(gdal_path, 0o755)
        if ret != 0:
            raise IOError(path)

    def makedirs(self, path, exist_ok=False):
        """Implements AbstractFileSystem.makedirs()"""

        gdal_path = self._get_gdal_path(path)
        if gdal.VSIStatL(gdal_path):
            if not exist_ok:
                raise FileExistsError(path)
            return

        self.mkdir(path, create_parents=True)

    def _rm(self, path):
        """Implements AbstractFileSystem._rm()"""

        gdal_path = self._get_gdal_path(path)
        ret = -1
        try:
            ret = gdal.Unlink(gdal_path)
        except Exception:
            pass
        if ret != 0:
            if gdal.VSIStatL(gdal_path) is None:
                raise FileNotFoundError(path)
            raise IOError(path)

    def rmdir(self, path):
        """Implements AbstractFileSystem.rmdir()"""

        gdal_path = self._get_gdal_path(path)
        ret = -1
        try:
            ret = gdal.Rmdir(gdal_path)
        except Exception:
            pass
        if ret != 0:
            if gdal.VSIStatL(gdal_path) is None:
                raise FileNotFoundError(path)
            raise IOError(path)

    def mv(self, path1, path2, recursive=False, maxdepth=None, **kwargs):
        """Implements AbstractFileSystem.mv()"""

        old_path = self._get_gdal_path(path1)
        new_path = self._get_gdal_path(path2)
        try:
            if gdal.MoveFile(old_path, new_path) != 0:
                if gdal.VSIStatL(old_path) is None:
                    raise FileNotFoundError(path1)
                raise IOError(f"Cannot move from {path1} to {path2}")
        except Exception:
            if gdal.VSIStatL(old_path) is None:
                raise FileNotFoundError(path1)
            raise

    def copy(
        self, path1, path2, recursive=False, maxdepth=None, on_error=None, **kwargs
    ):
        """Implements AbstractFileSystem.copy()"""

        old_path = self._get_gdal_path(path1)
        new_path = self._get_gdal_path(path2)
        try:
            if gdal.CopyFile(old_path, new_path) != 0:
                if gdal.VSIStatL(old_path) is None:
                    raise FileNotFoundError(path1)
                raise IOError(f"Cannot copy from {path1} to {path2}")
        except Exception:
            if gdal.VSIStatL(old_path) is None:
                raise FileNotFoundError(path1)
            raise


def register_vsi_implementations():
    """Register a generic "vsi" protocol and "vsimem", "vsitar", etc.
    This method is automatically called on osgeo.gdal_fsspec import.
    """
    register_implementation("vsi", VSIFileSystem)
    for vsi_prefix in gdal.GetFileSystemsPrefixes():
        if vsi_prefix.startswith("/vsi") and not vsi_prefix.endswith("?"):
            assert vsi_prefix.endswith("/")
            protocol = vsi_prefix[1:-1]
            # We need to duplicate the base class for each protocol, so that
            # each class has a distinct "protocol" member.
            new_class = type(
                "VSIFileSystem_" + protocol,
                VSIFileSystem.__bases__,
                dict(VSIFileSystem.__dict__),
            )
            register_implementation(protocol, new_class)


register_vsi_implementations()
