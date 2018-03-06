import os
import tempfile
from unittest import mock, TestCase

import gdal2tiles


class AttrDict(dict):
    def __init__(self, *args, **kwargs):
        super(AttrDict, self).__init__(*args, **kwargs)
        self.__dict__ = self


class OptionParserInputOutputTest(TestCase):

    def test_vanilla_input_output(self):
        _, input_file = tempfile.mkstemp()
        output_folder = tempfile.mkdtemp()
        parsed_input, parsed_output, options = gdal2tiles.process_args([input_file, output_folder])

        self.assertEqual(parsed_input, input_file)
        self.assertEqual(parsed_output, output_folder)
        self.assertNotEqual(options, {})

    def test_output_folder_is_the_input_file_folder_when_none_passed(self):
        _, input_file = tempfile.mkstemp()
        _, parsed_output, _ = gdal2tiles.process_args([input_file])

        self.assertEqual(parsed_output, os.path.basename(input_file))

    def _asserts_exits_with_code_2(self, params):
        with self.assertRaises(SystemExit) as cm:
            gdal2tiles.process_args(params)

        e = cm.exception
        self.assertEqual(str(e), '2')

    def test_exits_when_0_args_passed(self):
        self._asserts_exits_with_code_2([])

    def test_exits_when_more_than_2_free_parameters(self):
        self._asserts_exits_with_code_2(['input1.tiff', 'input2.tiff', 'output_folder'])

    def test_exits_when_input_file_does_not_exist(self):
        self._asserts_exits_with_code_2(['foobar.tiff'])

    def test_exits_when_first_param_is_not_a_file(self):
        folder = tempfile.gettempdir()
        self._asserts_exits_with_code_2([folder])


# pylint:disable=E1101
class OptionParserPostProcessingTest(TestCase):

    def setUp(self):
        self.DEFAULT_OPTIONS = {
            'verbose': True,
            'resampling': 'near',
            'title': '',
            'url': '',
        }
        self.DEFAULT_ATTRDICT_OPTIONS = AttrDict(self.DEFAULT_OPTIONS)

    def _setup_gdal_patch(self, mock_gdal):
        mock_gdal.TermProgress_nocb = True
        mock_gdal.RegenerateOverview = True
        mock_gdal.GetCacheMax = lambda: 1024 * 1024
        return mock_gdal

    def test_title_is_untouched_if_set(self):
        title = "fizzbuzz"
        self.DEFAULT_ATTRDICT_OPTIONS['title'] = title

        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, "bar.tiff", "baz")

        self.assertEqual(options.title, title)

    def test_title_default_to_input_filename_if_not_set(self):
        input_file = "foo/bar/fizz/buzz.tiff"

        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, input_file, "baz")

        self.assertEqual(options.title, os.path.basename(input_file))

    def test_url_stays_empty_if_not_passed(self):
        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "baz")

        self.assertEqual(options.url, "")

    def test_url_ends_with_the_output_folder_last_component(self):
        output_folder = "foo/bar/fizz"
        url = "www.mysite.com/storage"
        self.DEFAULT_ATTRDICT_OPTIONS['url'] = url

        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", output_folder)

        self.assertEqual(options.url, url + "/fizz/")

        # With already present trailing slashes
        output_folder = "foo/bar/fizz/"
        url = "www.mysite.com/storage/"
        self.DEFAULT_ATTRDICT_OPTIONS['url'] = url

        options = gdal2tiles.options_post_processing(
            self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", output_folder)

        self.assertEqual(options.url, url + "fizz/")

    @mock.patch('gdal2tiles.gdal', spec=AttrDict())
    def test_average_resampling_supported_with_latest_gdal(self, mock_gdal):
        self._setup_gdal_patch(mock_gdal)
        self.DEFAULT_ATTRDICT_OPTIONS['resampling'] = "average"

        gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")
        # No error means it worked as expected

    @mock.patch('gdal2tiles.gdal', spec=AttrDict())
    def test_average_resampling_not_supported_in_old_gdal(self, mock_gdal):
        mock_gdal = self._setup_gdal_patch(mock_gdal)
        del mock_gdal.RegenerateOverview
        self.DEFAULT_ATTRDICT_OPTIONS['resampling'] = "average"

        with self.assertRaises(SystemExit):
            gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")

    def test_antialias_resampling_supported_with_numpy(self):
        gdal2tiles.numpy = True
        self.DEFAULT_ATTRDICT_OPTIONS['resampling'] = "antialias"

        gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")
        # No error means it worked as expected

    def test_antialias_resampling_not_supported_wout_numpy(self):
        if hasattr(gdal2tiles, "numpy"):
            del gdal2tiles.numpy

        self.DEFAULT_ATTRDICT_OPTIONS['resampling'] = "antialias"

        with self.assertRaises(SystemExit):
            gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "foo.tiff", "/bar/")
