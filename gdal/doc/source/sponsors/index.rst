.. _sponsors:

================================================================================
Sponsors
================================================================================

GDAL is a truly collaborative effort, with many diverse organizations
contributing resources to its success. The following organizations take an
extra step, providing unrestricted funding every year to maintain and improve
the health of the project:

- Gold level:

  .. _gold-sponsors:
  .. container:: horizontal-logos

    .. Note: they will appear in randomized order in HTML

    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-maxar.png
           :class: img-logos
           :width: 250 px
           :target: https://www.maxar.com

    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-microsoft.png
           :class: img-logos
           :width: 250 px
           :target: https://www.microsoft.com

    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-planet.png
           :class: img-logos
           :width: 250 px
           :target: https://www.planet.com


- Silver level:

  .. _silver-sponsors:
  .. container:: horizontal-logos

    .. Note: they will appear in randomized order in HTML

    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-esri.png
           :class: img-logos
           :width: 200 px
           :target: https://www.esri.com

    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-google.png
           :class: img-logos
           :width: 200 px
           :target: https://www.google.com

    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-safe.png
           :class: img-logos
           :width: 200 px
           :target: https://www.safe.com



- Bronze level:

  .. _bronze-sponsors:
  .. container:: horizontal-logos

    .. Note: they will appear in randomized order in HTML

    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-frontiersi.png
           :class: img-logos
           :width: 150 px
           :target: https://frontiersi.com.au


    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-koordinates.png
           :class: img-logos
           :width: 150 px
           :target: https://www.koordinates.com


    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-mapgears.png
           :class: img-logos
           :width: 150 px
           :target: https://www.mapgears.com


    .. container:: horizontal-logo

        .. image:: ../../images/sponsors/logo-sparkgeo.png
           :class: img-logos
           :width: 150 px
           :target: https://www.sparkgeo.com


- Supporter level:

  .. _supporter-sponsors:
  .. container:: horizontal-logos

    .. container:: horizontal-logo

        Myles Sutherland

    .. container:: horizontal-logo

        `Space Intelligence <https://www.space-intelligence.com>`__

    .. container:: horizontal-logo

        `Umbra <https://umbra.space/>`__

.. raw:: html

   <script type="text/javascript">
    // Randomize logos
    $.fn.randomize = function(selector){
        var $elems = selector ? $(this).find(selector) : $(this).children(),
            $parents = $elems.parent();

        // found at: http://stackoverflow.com/a/2450976/746961
        function shuffle(array) {
            var currentIndex = array.length, temporaryValue, randomIndex;
            // While there remain elements to shuffle...
            while (0 !== currentIndex) {
                // Pick a remaining element...
                randomIndex = Math.floor(Math.random() * currentIndex);
                currentIndex -= 1;

                // And swap it with the current element.
                temporaryValue = array[currentIndex];
                array[currentIndex] = array[randomIndex];
                array[randomIndex] = temporaryValue;
            }
            return array;
        }

        $parents.each(function(){
            var elements = $(this).children(selector);
            shuffle(elements);
            $(this).append(elements);
        });

        return this;
    };
    $('#gold-sponsors').randomize('div.horizontal-logo');
    $('#silver-sponsors').randomize('div.horizontal-logo');
    $('#bronze-sponsors').randomize('div.horizontal-logo');
    $('#supporter-sponsors').randomize('div.horizontal-logo');

  </script>

The GDAL Project is hosted by `OSGeo <https://www.osgeo.org>`__,
and a fiscally sponsored project of `NumFOCUS <https://numfocus.org>`__, a
nonprofit dedicated to supporting the open-source scientific computing community. If you
like GDAL and want to support our mission, please consider making a
`donation <https://numfocus.org/donate-to-gdal>`__ to support our efforts.

NumFOCUS is 501(c)(3) non-profit charity in the United States; as such, donations to
NumFOCUS are tax-deductible as allowed by law. As with any donation, you should
consult with your personal tax adviser or the IRS about your particular tax situation.

.. container:: horizontal-logos

    .. container:: horizontal-logo

        .. image:: ../../images/logo-osgeo.png
           :class: img-logos
           :width: 150 px
           :target: https://www.osgeo.org

    .. container:: horizontal-logo

        .. image:: ../../images/logo-numfocus.png
           :class: img-logos
           :width: 150 px
           :target: https://numfocus.org

Sponsoring
----------

If your organization benefits from GDAL we recommend joining the group of
sponsors above to "pay it forward" and ensure the project has the resources to
stay healthy and grow. To learn about the benefits of becoming a sponsor at
various levels start with the `Sustainable GDAL Sponsorship Prospectus`_.
If you are interested, need help convincing your key decision-makers, or have
any questions, don't hesitate to contact gdal-sponsors@osgeo.org.

Related resources
-----------------

- `Sustainable GDAL Sponsorship Prospectus`_.
- :ref:`Sponsoring frequently asked questions (FAQ) <sponsoring-faq>`.

.. Source of the PDF is at https://docs.google.com/document/d/1yhMWeI_LgEXPUkngqOitqcKfp7ov6WsS41v5ulz-kd0/edit#

.. _Sustainable GDAL Sponsorship Prospectus: https://gdal.org/sponsors/Sustainable%20GDAL%20Sponsorship%20Prospectus.pdf

..
    Developer comment: make html includes a hack to hide the table from
    the index.html file. We need to keep it visible so that the top-level
    index.html lists those pages.

.. toctree::
   :maxdepth: 0

   faq
