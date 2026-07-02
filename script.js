// FS Selection
const fsButtons = document.querySelectorAll(".fs");

fsButtons.forEach(btn => {
    btn.addEventListener("click", () => {
        fsButtons.forEach(b => b.classList.remove("active"));
        btn.classList.add("active");
        updateFreeLabels();
    });
});

// MODE TAB
const modeButtons = document.querySelectorAll(".mode");
const snapPanel = document.getElementById("snapPanel");
const freePanel = document.getElementById("freePanel");

modeButtons.forEach(btn => {
    btn.addEventListener("click", () => {

        modeButtons.forEach(b => b.classList.remove("active"));
        btn.classList.add("active");

        if (btn.innerText.includes("SNAP")) {
            snapPanel.classList.add("active");
            freePanel.classList.remove("active");
        } else {
            freePanel.classList.add("active");
            snapPanel.classList.remove("active");
        }
    });
});

// SETTINGS
const dialog = document.getElementById("settingsDialog");
document.querySelector(".setting-btn").onclick = () => {
    dialog.style.display = "flex";
};

document.querySelector(".dialog-close").onclick = () => {
    dialog.style.display = "none";
};

document.querySelector(".cancel-btn").onclick = () => {
    dialog.style.display = "none";
};

dialog.onclick = (e) => {
    if (e.target === dialog) dialog.style.display = "none";
};

// SAVE
document.getElementById("saveButton").onclick = () => {
    alert("Save to Device");
};

// FREE LABEL
const freeBadges = document.querySelectorAll(".free-card .fs-badge");

function updateFreeLabels() {

    const active = document.querySelector(".fs.active .fs-num").innerText;
    const current = parseInt(active.replace("FS", ""));

    let arr = [];

    for (let i = 1; i <= 4; i++) {
        if (i !== current) arr.push("FS" + i);
    }

    freeBadges.forEach((el, i) => {
        el.innerText = arr[i];
    });
}

updateFreeLabels();

// INPUT LIMIT
document.querySelectorAll("input[type='number']").forEach(input => {
    if (!input.min) input.min = 0;
    if (!input.max) input.max = 127;
});

// DEMO DATA
pc1.value = 0;
pc2.value = 3;
cc1.value = 40;
cc2.value = 50;
on1.value = 127;
on2.value = 127;
off1.value = 0;
off2.value = 0;

// DEMO: skip splash straight to app (remove in production)
// document.getElementById("splash").style.display = "none";
// document.getElementById("app").classList.add("show");