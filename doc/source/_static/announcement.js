document.addEventListener("DOMContentLoaded",
function()
{
    var banner = document.createElement("div");
    banner.id = "announcement-banner";
    banner.innerHTML = "The GDAL project is currently soliciting feedback to help focus activities.<br>We would highly appreciate you fill in the <a href=\"https://gdal.org/survey/\"><strong>survey</strong></a> that will provide guidance about priorities for the program's resources (open until end of December 2025).<br>Five T-shirts will be distributed to randomly chosen respondents who leave their email!";
    var body = document.body;
    body.insertBefore(banner, body.firstChild);
});
