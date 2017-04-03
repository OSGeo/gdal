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
        _, parsed_output, options = gdal2tiles.process_args([input_file])

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


class OptionParserPostProcessingTest(TestCase):

    def setUp(self):
        self.DEFAULT_OPTIONS = {'verbose': True, 'resampling': 'near'}
        self.DEFAULT_ATTRDICT_OPTIONS = AttrDict(self.DEFAULT_OPTIONS)

    @mock.patch('gdal2tiles.gdal', spec=[])
    def test_verbose_resampling_does_not_need_TermProgress_nocb(self, mock_term_progress):
        # With the [] spec, calling any non-list method/property on gdal will raise an error, which
        # is what we want gdal.TermProgress_nocb to do
        gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "bar.tiff", "baz")
        # The fact that no error is thrown is the point of the test

    @mock.patch('gdal2tiles.gdal', spec=[])
    def test_non_verbose_resampling_exits_if_no_TermProgress_nocb(self, _):
        # With the [] spec, calling any non-list method/property on gdal will raise an error, which
        # is what we want gdal.TermProgress_nocb to do
        self.DEFAULT_ATTRDICT_OPTIONS['verbose'] = False

        with self.assertRaises(SystemExit):
            gdal2tiles.options_post_processing(self.DEFAULT_ATTRDICT_OPTIONS, "bar.tiff", "baz")
