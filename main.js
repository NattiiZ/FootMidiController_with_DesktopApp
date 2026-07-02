const { app, BrowserWindow } = require("electron");
const { SerialPort } = require("serialport");
const { ReadlineParser } = require("@serialport/parser-readline");
const { list } = require("serialport");

require("electron-reload")(__dirname, {
    electron: require(`${__dirname}/node_modules/electron`)
});

let win;

const WINDOW_NORMAL = { width: 1100, height: 750 };
const WINDOW_LOADING = { width: 420, height: 300 };

// ==========================================
// CONFIG
// ==========================================
const BAUD = 115200;
const BOOT_DELAY = 3200;
const PING_COUNT = 5;
const PING_INTERVAL = 150;
const RESCAN_DELAY = 1500;

// ==========================================
// RUNTIME
// ==========================================
let connectedPort = null;
let connectedPath = null;
let scanning = false;
let connecting = false;
// ==========================================
// UTIL
// ==========================================
const delay = (ms) => new Promise(r => setTimeout(r, ms));

// ==========================================
// WINDOW
// ==========================================
function createWindow() {
    const { ipcMain } = require("electron");

    win = new BrowserWindow({
        width:1100,
        height:750,

        frame:false,
        autoHideMenuBar:true,

        titleBarStyle:"hidden",

        backgroundColor:"#141414",

        webPreferences:{
            nodeIntegration:true,
            contextIsolation:false
        }
    });

    win.loadFile("index.html");
}

const { ipcMain } = require("electron");

ipcMain.on("window-minimize", () => {
    win.minimize();
});


ipcMain.on("window-close", () => {
    win.close();
});


// ==========================================
// GET PORTS
// ==========================================
async function getPorts() {
    const ports = await SerialPort.list();

    return ports
        .map(p => p.path)
        .sort((a, b) => {
            const na = parseInt(a.replace("COM", "")) || 0;
            const nb = parseInt(b.replace("COM", "")) || 0;
            return nb - na;
        });
}

// ==========================================
// SCAN LOOP
// ==========================================


async function scanOnce() {

    win.setResizable(false);
    win.setSize(WINDOW_LOADING.width, WINDOW_LOADING.height, true);
    win.center();


    if (scanning || waitingReconnect) return;

    scanning = true;

    

    send("searching");

    const ports = await getPorts();

    if (!ports.length) {

        scanning = false;

        send("notfound");

        return;
    }

    for (const path of ports) {

        const port = await probePort(path);

        if (port) {

            connectedPort = port;
            connectedPath = path;

            scanning = false;
            waitingReconnect = false;

            connecting = true;

            // เริ่มฟัง close ตั้งแต่ตอนนี้
            startReadLoop(port);

            await fakeConnectingAnimation();

            // ถ้าหลุดระหว่าง fake loading
            if (!connectedPort)
                return;

            connecting = false;

            send("connected", {
                port: connectedPath
            });

            return;
        }
    }

    scanning = false;
    send("notfound");
}



// ==========================================
// PROBE DEVICE
// ==========================================
async function probePort(path) {

    return new Promise((resolve) => {

        let buffer = "";
        let finished = false;

        const port = new SerialPort({
            path,
            baudRate: BAUD,
            autoOpen: false
        });

        function done(result) {
            if (finished) return;
            finished = true;
            resolve(result);
        }

        port.open(async (err) => {

            if (err) return done(null);

            await delay(BOOT_DELAY);

            port.flush(() => {});

            for (let i = 0; i < PING_COUNT; i++) {

                port.write("PING_FM4\n");

                const start = Date.now();

                while (Date.now() - start < PING_INTERVAL) {

                    if (buffer.includes("PONG_FM4")) {

                        port.removeAllListeners();
                        return done(port); // 🔥 จบเลย ไม่ทำ UI
                    }

                    await delay(10);
                }
            }

            try {
                port.close(() => done(null));
            } catch {
                done(null);
            }
        });

        port.on("data", (d) => buffer += d.toString());
        port.on("error", () => {});
    });
}

// ==========================================
// DEVICE LOOP
// ==========================================
function startReadLoop(port) {

    port.on("data", (d) => {
        console.log("[DEVICE]", d.toString().trim());
    });

    port.on("close", async () => {

        console.log("Device disconnected");

        connectedPort = null;
        connectedPath = null;

        // หลุดระหว่าง Connecting
        if (connecting) {

            connecting = false;

            send("retry");

            await delay(2000);

            await scanOnce();

            return;
        }

        // หลุดหลัง Connected
        send("notfound");

    });

    port.on("error", (err) => {
        console.log("[DEVICE ERROR]", err.message);
    });
}

// ==========================================
// START APP
// ==========================================


app.whenReady().then(async () => {
    createWindow();

    startUSBWatch(); // 🔥 สำคัญ

    await delay(1000);
    // send("searching");
    scanOnce();
});

app.on("window-all-closed", () => {
    try {
        if (connectedPort?.isOpen) connectedPort.close();
    } catch {}

    app.quit();
});


function send(status, extra = {}) {
  win.webContents.send("device-status", {
      status,
      ...extra
  });
}


async function fakeConnectingAnimation() {

    // 1) โชว์ 0% ค้างก่อน
    await delay(1000);

    send("connecting", {
        port: connectedPath,
        progress: 0
    });


    await delay(1000); // 🔥 จุดที่มึงต้องการ (hold 1 วิ)

    // 2) ค่อยเริ่มวิ่ง
    let p = 0;

    while (p < 100) {

        p += Math.random() * 10 + 4; // smooth worm feel

        if (p > 100) p = 100;

        
        send("connecting", {
            port: connectedPath,
            progress: Math.floor(p)
        });

        await delay(30);
    }

    // 3) hold เล็กน้อยก่อนเข้า app
    await delay(500);

    win.setSize(WINDOW_NORMAL.width, WINDOW_NORMAL.height, true);
    win.center();
    win.setResizable(true);
}


let watching = false;
let bootLock = false;
let waitingReconnect = false;

function startUSBWatch() {

    if (watching) return;
    watching = true;

    let lastPorts = null;   // <-- เปลี่ยนจาก []

    setInterval(async () => {

        const ports = await SerialPort.list();
        const current = ports.map(p => p.path).sort();

        // รอบแรก แค่จำค่าไว้
        if (lastPorts === null) {
            lastPorts = current;
            return;
        }

        const added = current.filter(p => !lastPorts.includes(p));

        const removed = lastPorts.filter(p => !current.includes(p));

        if (!added.length && !removed.length)
            return;

        console.log("[USB CHANGE]");

        lastPorts = current;

        // ถอด USB อย่างเดียว -> ไม่ต้องทำอะไร
        if (removed.length && !added.length) {

            console.log("USB Removed");

            return;
        }

        // เสียบ USB -> ค่อยค้นหา
        if (added.length) {

            console.log("USB Added");

            if (connectedPort) return;
            if (waitingReconnect) return;

            waitingReconnect = true;
            scanning = false;

            setTimeout(async () => {

                waitingReconnect = false;

                await scanOnce();

            }, 2500); // รอ Arduino Boot

        }

    }, 800);
}