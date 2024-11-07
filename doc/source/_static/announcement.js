document.addEventListener("DOMContentLoaded",
function()
{
    var banner = document.createElement("div");
    banner.id = "announcement-banner";
    banner.innerHTML = "The GDAL project is currently soliciting feedback to help focus <a href=\"https://gdal.org/en/latest/development/rfc/rfc83_use_of_project_sponsorship.html\">GDAL Sponsorship Program</a> activities.<br>We would highly appreciate you fill in the <a href=\"https://docs.google.com/forms/d/e/1FAIpQLSdMXygtDb5M0Ov0KK0u2wKkev1rMqAjRdTlwMeCl7Z1TGJTLw/viewform\"><strong>survey</strong></a> that will provide guidance about priorities for the program's resources (open until November 11th, 2024).";
    var body = document.body;
    body.insertBefore(banner, body.firstChild);
});
