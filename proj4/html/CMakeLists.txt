set(HTML_DOC_FILE faq.html
                  gen_parms.html
                  index.html
                  man_cs2cs.html
                  man_geod.html
                  man_pj_init.html
                  man_proj.html
                  proj_logo.png)
                  
install(FILES ${HTML_DOC_FILE} 
               DESTINATION ${DOCDIR}
               PERMISSIONS WORLD_READ WORLD_WRITE)
