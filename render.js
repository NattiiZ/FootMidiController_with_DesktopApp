const { ipcRenderer } = require("electron");

const statusText = document.getElementById("statusText");
const splash = document.getElementById("splash");
const appDiv = document.getElementById("app");

const progressWrap = document.getElementById("progressWrap");
const progressBar = document.getElementById("progressBar");

document.getElementById("btnMin").onclick = () =>
    ipcRenderer.send("window-minimize");

document.getElementById("btnClose").onclick = () =>
    ipcRenderer.send("window-close");

ipcRenderer.send("window-resize-main");

let holdConnected = false;

function setSearchingUI() {

    progressWrap.classList.add("searching");
    progressWrap.style.display = "block";

    progressBar.style.width = "100%"; // worm ใช้ animation แทน width
}

function setProgressUI(p) {

    progressWrap.classList.remove("searching");
    progressWrap.style.display = "block";

    progressBar.style.width = (p || 0) + "%";
}

function hideProgress() {
    progressWrap.style.display = "none";
}

ipcRenderer.on("device-status", (event, data) => {

    const { status, port, progress } = data;

    splash.style.border = "none";

    // 🔍 SEARCHING = worm วิ่ง
    if (status === "searching") {

        holdConnected = false;

        statusText.innerText = "Searching device...";

        setSearchingUI();
    }

    // 🔌 CONNECTING = progress จริง
    if (status === "connecting") {

        statusText.innerText = "Connecting...";

        setProgressUI(progress);
    }

    // ✅ CONNECTED
    if (status === "connected") {

        if (holdConnected) return;
        holdConnected = true;

        statusText.innerText = "Connected";

        setProgressUI(100);

        setTimeout(() => {

            splash.style.opacity = "0";

            setTimeout(() => {
                splash.style.display = "none";
                appDiv.classList.add("show");
                
            }, 300);

        }, 1000);
    }

    // ❌ NOT FOUND
    if (status === "notfound") {

        holdConnected = false;

        statusText.innerText = "Device not found";

        hideProgress();

        splash.style.border = "2px solid #ff4d4d";
    }

    if (status === "retry") {

        holdConnected = false;

        statusText.innerText =
            "Attempting to reconnect...";

        setSearchingUI();
    }
});

const connectionText =
document.getElementById("connectionText");

connectionText.innerText =
"Connected";

connectionText.innerText =
"Searching...";

connectionText.innerText =
"Device Not Found";

connectionText.innerText =
"Reconnecting...";


function setSplashSmall() {
    splash.classList.add("small");
    splash.classList.remove("hide");
}

function hideSplash() {
    splash.classList.add("hide");

    setTimeout(() => {
        splash.style.display = "none";
        appDiv.classList.add("show");
    }, 350);
}

