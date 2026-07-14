let trackedWindow = null;

function publishGeometry() {
    const window = workspace.activeWindow;
    if (!window) {
        callDBus("org.smarttype.UI", "/SmartTypeUI", "org.smarttype.UI",
                 "SetActiveWindowGeometry", 0, 0, 0, 0);
        return;
    }
    const geometry = window.frameGeometry;
    if (geometry) {
        callDBus("org.smarttype.UI", "/SmartTypeUI", "org.smarttype.UI",
                 "SetActiveWindowGeometry",
                 Math.round(geometry.x), Math.round(geometry.y),
                 Math.round(geometry.width), Math.round(geometry.height));
    }
}

function trackActiveWindow() {
    if (trackedWindow) {
        try {
            trackedWindow.frameGeometryChanged.disconnect(publishGeometry);
        } catch(e) {}
    }
    trackedWindow = workspace.activeWindow;
    if (trackedWindow) {
        trackedWindow.frameGeometryChanged.connect(publishGeometry);
    }
    publishGeometry();
}

workspace.windowActivated.connect(trackActiveWindow);
trackActiveWindow();

// Периодический опрос через QTimer (setInterval не поддерживается в KWin)
var pollTimer = new QTimer();
pollTimer.interval = 500;
pollTimer.timeout.connect(publishGeometry);
pollTimer.start();
