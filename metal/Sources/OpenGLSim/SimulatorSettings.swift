import Foundation
import os

enum LogLevel: Int, CaseIterable {
    case debug = 0, info, warning, error, critical
}

final class SimulatorLogger {
    static let shared = SimulatorLogger()
    private let lock = NSLock()
    private let logger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "opengl-sim", category: "simulator")
    private var file: FileHandle?
    var level: LogLevel = .info

    private init() {
        do {
            let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
            let preferred = root.appendingPathComponent("logs", isDirectory: true)
            let directory: URL
            do {
                try FileManager.default.createDirectory(at: preferred, withIntermediateDirectories: true)
                directory = preferred
            } catch {
                directory = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
                    .appendingPathComponent("OpenGLSim/logs", isDirectory: true)
            }
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            let url = directory.appendingPathComponent("simulator.log")
            if !FileManager.default.fileExists(atPath: url.path) {
                FileManager.default.createFile(atPath: url.path, contents: nil)
            }
            file = try FileHandle(forWritingTo: url)
            try file?.seekToEnd()
        } catch {
            logger.error("Could not open simulator log: \(error.localizedDescription, privacy: .public)")
        }
    }

    func write(_ level: LogLevel, _ message: String) {
        guard level.rawValue >= self.level.rawValue else { return }
        let names = ["DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"]
        let line = "[\(ISO8601DateFormatter().string(from: Date()))] [\(names[level.rawValue])] \(message)\n"
        lock.lock()
        defer { lock.unlock() }
        if let data = line.data(using: .utf8) { try? file?.write(contentsOf: data) }
        switch level {
        case .debug: logger.debug("\(message, privacy: .public)")
        case .info: logger.info("\(message, privacy: .public)")
        case .warning: logger.warning("\(message, privacy: .public)")
        case .error, .critical: logger.error("\(message, privacy: .public)")
        }
    }
}

struct SimulatorSettings {
    var logLevel: LogLevel = .info
    var windowWidth = 1280
    var windowHeight = 800

    static func load() -> SimulatorSettings {
        var settings = SimulatorSettings()
        let url = Bundle.module.url(forResource: "simulator", withExtension: "conf") ??
            URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("config/simulator.conf")
        guard let contents = try? String(contentsOf: url, encoding: .utf8) else {
            SimulatorLogger.shared.write(.warning, "No config/simulator.conf found; using defaults.")
            return settings
        }
        for line in contents.split(whereSeparator: \.isNewline) {
            let parts = line.split(separator: "=", maxSplits: 1).map { $0.trimmingCharacters(in: .whitespaces) }
            guard parts.count == 2 else { continue }
            switch parts[0] {
            case "log_level":
                switch parts[1].lowercased() {
                case "debug": settings.logLevel = .debug
                case "warning", "warn": settings.logLevel = .warning
                case "error": settings.logLevel = .error
                case "critical": settings.logLevel = .critical
                default: settings.logLevel = .info
                }
            case "window_width": settings.windowWidth = Int(parts[1]) ?? settings.windowWidth
            case "window_height": settings.windowHeight = Int(parts[1]) ?? settings.windowHeight
            default: break
            }
        }
        SimulatorLogger.shared.level = settings.logLevel
        SimulatorLogger.shared.write(.info, "Loaded simulator settings from \(url.path)")
        return settings
    }
}
