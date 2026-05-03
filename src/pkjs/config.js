var stored = localStorage.getItem("matrix_settings");
if (stored) {
    var s = JSON.parse(stored);
    document.getElementById("hostserver").value = s.hostserver || "";
    document.getElementById("user").value = s.user || "";
    document.getElementById("pass").value = s.pass || "";
}

document.getElementById("save").addEventListener("click", function() {
    var settings = {
        hostserver: document.getElementById("hostserver").value.trim(),
        user: document.getElementById("user").value.trim(),
        pass: document.getElementById("pass").value.trim()
    };

    localStorage.setItem("matrix_settings", JSON.stringify(settings));

    // Return settings to Pebble app
    var returnToPebble = "pebblejs://close#" +
    encodeURIComponent(JSON.stringify(settings));

    document.location = returnToPebble;
});