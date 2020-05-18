# Boundless Read the Docs Theme

Operates as an extension of ``sphinx_rtd_theme`` originated by Boundless

Reference:

* https://github.com/rtfd/sphinx_rtd_theme

## Configuration

Configuration options provided by ``boundess_rtd`:

```
html_theme_options = {
    'display_connect': False #Ask a Question
}
```

Configuration options provided by `sphinx_rtd_theme`:

```
html_theme_options = {
    'canonical_url': '',
    'analytics_id': '',
    'logo_only': False,
    'display_version': True,
    'prev_next_buttons_location': 'both',
    'style_external_links': True,
    'vcs_pageview_mode': '',
    # Toc options
    'collapse_navigation': False,
    'sticky_navigation': True,
    'navigation_depth': 3,
    'includehidden': True,
    'titles_only': False
}
```

For the navigation bar to work correctly be sure to include captions in your `tocreee` definitions.

## Implementation

Care is taken to override the `sphinx_rtd_theme` CSS settings as required, rather than duplicate the files. This approach allows us to update `sphinx_rtd_theme` over time to take advantage features and fixes.

The following is an outline of `boundless_rtd` contents:

* static/css/boundless.css - provides font selection and overrides branding elements
* static/fonts/proximanova-*
* static/fonts/sourcecodepro-*
* static/img/boundless-logo-32.png
* static/img/powered-by-boundless*.png
* static/js/
