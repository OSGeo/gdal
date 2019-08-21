from docutils import nodes

def setup(app):
    app.add_role('dsco', configoption('%s'))
    app.add_role('lco', configoption('%s'))
    app.add_role('co', configoption('%s'))
    app.add_role('oo', configoption('%s'))
    return { 'parallel_read_safe': False, 'parallel_write_safe': True }

def configoption(pattern):
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):
        url = pattern % (text,)
        node = nodes.reference(rawtext, text, refuri=url, **options)
        return [node], []
    return role
