".syntax version_directive;\n"
"version_directive\n"
"	version_directive_1 .and .loop version_directive_2;\n"
"version_directive_1\n"
"	prior_optional_spaces .and optional_version_directive .and .true .emit $;\n"
"version_directive_2\n"
"	prior_optional_spaces .and version_directive_body .and .true .emit $;\n"
"optional_version_directive\n"
"	version_directive_body .or .true .emit 10 .emit 1;\n"
"version_directive_body\n"
"	'#' .and optional_space .and \"version\" .and space .and version_number .and optional_space .and\n"
"	new_line;\n"
"version_number\n"
"	version_number_110;\n"
"version_number_110\n"
"	leading_zeroes .and \"110\" .emit 10 .emit 1;\n"
"leading_zeroes\n"
"	.loop zero;\n"
"zero\n"
"	'0';\n"
"space\n"
" single_space .and .loop single_space;\n"
"optional_space\n"
" .loop single_space;\n"
"single_space\n"
" ' ' .or '\\t';\n"
"prior_optional_spaces\n"
"	.loop prior_space;\n"
"prior_space\n"
"	c_style_comment_block .or cpp_style_comment_block .or space .or new_line;\n"
"c_style_comment_block\n"
" '/' .and '*' .and c_style_comment_rest;\n"
"c_style_comment_rest\n"
" .loop c_style_comment_char_no_star .and c_style_comment_rest_1;\n"
"c_style_comment_rest_1\n"
" c_style_comment_end .or c_style_comment_rest_2;\n"
"c_style_comment_rest_2\n"
" '*' .and c_style_comment_rest;\n"
"c_style_comment_char_no_star\n"
" '\\x2B'-'\\xFF' .or '\\x01'-'\\x29';\n"
"c_style_comment_end\n"
" '*' .and '/';\n"
"cpp_style_comment_block\n"
" '/' .and '/' .and cpp_style_comment_block_1;\n"
"cpp_style_comment_block_1\n"
" cpp_style_comment_block_2 .or cpp_style_comment_block_3;\n"
"cpp_style_comment_block_2\n"
" .loop cpp_style_comment_char .and new_line;\n"
"cpp_style_comment_block_3\n"
" .loop cpp_style_comment_char;\n"
"cpp_style_comment_char\n"
" '\\x0E'-'\\xFF' .or '\\x01'-'\\x09' .or '\\x0B'-'\\x0C';\n"
"new_line\n"
" cr_lf .or lf_cr .or '\\n' .or '\\r';\n"
"cr_lf\n"
" '\\r' .and '\\n';\n"
"lf_cr\n"
"	'\\n' .and '\\r';\n"
".string __string_filter;\n"
"__string_filter\n"
" .loop __identifier_char;\n"
"__identifier_char\n"
" 'a'-'z' .or 'A'-'Z' .or '_' .or '0'-'9';\n"
""

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
