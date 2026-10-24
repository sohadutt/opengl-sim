import AppKit

@main
@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private var window: NSWindow?

    func applicationDidFinishLaunching(_ notification: Notification) {
        let settings = SimulatorSettings.load()
        let controller = SimulationViewController()
        let window = NSWindow(contentViewController: controller)
        window.title = "OpenGL Sim — Metal 4"
        window.setContentSize(NSSize(width: settings.windowWidth, height: settings.windowHeight))
        window.minSize = NSSize(width: 900, height: 600)
        window.center()
        window.makeKeyAndOrderFront(nil)
        self.window = window
        NSApp.activate(ignoringOtherApps: true)
        SimulatorLogger.shared.write(.info, "Application launched.")
    }

    func applicationWillTerminate(_ notification: Notification) {
        SimulatorLogger.shared.write(.info, "Application shutting down.")
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}
