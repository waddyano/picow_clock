#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "ota.h"

// Note this has two %s substitutions bot for the title
static const char *update_page = R"!(<html>
	<head>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
		<title>%s</title>
		<script>
function startUpload() {
    var otafile = document.getElementById("otafile").files;

    if (otafile.length == 0) {
        alert("No file selected!");
    } else {
        document.getElementById("otafile").disabled = true;
        document.getElementById("upload").disabled = true;

        var file = otafile[0];
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    document.open();
                    document.write(xhr.responseText);
                    document.close();
                } else if (xhr.status == 0) {
                    alert("Server closed the connection abruptly!");
                    location.reload()
                } else {
                    alert(xhr.status + " Error!\n" + xhr.responseText);
                    location.reload()
                }
            }
        };

        xhr.upload.onprogress = function (e) {
            let percent = (e.loaded / e.total * 100).toFixed(0);
            let progress2 = document.getElementById("progress2");
            progress2.textContent = "" + percent + "%";
            let progress = document.getElementById("progress");
            progress.value = percent;
            progress.textContent = "" + percent + "%";
        };
        xhr.open("POST", "/post_update", true);
        xhr.send(file);
    }
}
		</script>
<style>
body, textarea, label, button, input[type=file] {font-family: arial, sans-serif;}
label, input[type=file] { line-height:2.4rem; font-size:1.2rem; }
input::file-selector-button, button { border: 0; border-radius: 0.3rem; background:#1fa3ec; color:#ffffff; line-height:2.4rem; font-size:1.2rem; width:180px;
-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#0b73aa;}
</style>
	</head>
	<body>
		<h1 style="text-align: center">%s</h1>
        <div style="margin-left:auto; margin-right: auto; display: table;">
            <div style="padding: 6px">
                <label for="otafile" class="file">Update firmware:&nbsp</label>
                <input type="file" id="otafile" name="otafile" />
            </div>
            <span>
                <button id="upload" type="button" onclick="startUpload()">Upload</button>
            </span><span>
            <progress id="progress" value="0" max="100"></progress></span>
            <span id="progress2"></span>
        </div>
	</body>
</html>)!";

char *ota_get_update_page(const char *title)
{
    size_t len = strlen(update_page) + 2 * strlen(title);
    char *page = (char *)malloc(len); // %s is 2 characters
    if (page == nullptr)
    {
        return nullptr;
    }
    snprintf(page, len, update_page, title, title);
    return page;
}