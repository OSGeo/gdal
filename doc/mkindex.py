import string

orig_doc = open('html/index.html').read()

replace_me_off = string.find(orig_doc,'REPLACE_ME')

header = orig_doc[:replace_me_off]

final_doc = header + open('doc/index_raw.html').read()

open('html/index.html','w').write(final_doc)



