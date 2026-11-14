import Foundation
import GameController

// Both input sources contribute to the same pad. Releasing a touch must not
// release a button still held on a controller, or the other way around.
final class ControllerInput {
    static let shared = ControllerInput()
    private var observers: [NSObjectProtocol] = []
    private var controller: GCController?
    private var touch: UInt32 = 0
    private var physical: UInt32 = 0
    private var active = false

    func start() {
        guard observers.isEmpty else { return }
        for name in [NSNotification.Name.GCControllerDidConnect, .GCControllerDidDisconnect] {
            observers.append(NotificationCenter.default.addObserver(forName: name,
                              object: nil, queue: .main) { [weak self] _ in self?.connect() })
        }
        connect()
    }

    func stop() {
        observers.forEach { NotificationCenter.default.removeObserver($0) }
        observers.removeAll()
        controller?.extendedGamepad?.valueChangedHandler = nil
        controller = nil
        setActive(false)
    }

    func setActive(_ value: Bool) {
        active = value
        touch = 0
        readController()
    }

    func setTouch(_ mask: UInt32) {
        touch = mask
        publish()
    }

    private func connect() {
        controller?.extendedGamepad?.valueChangedHandler = nil
        controller = GCController.controllers().first { $0.extendedGamepad != nil }
        controller?.handlerQueue = .main
        controller?.extendedGamepad?.valueChangedHandler = { [weak self] _, _ in
            self?.readController()
        }
        readController()
    }

    private func readController() {
        physical = 0
        if let pad = controller?.extendedGamepad {
            for stick in [pad.dpad, pad.leftThumbstick] {
                if stick.xAxis.value < -0.35 { physical |= R3_PAD_LEFT }
                if stick.xAxis.value >  0.35 { physical |= R3_PAD_RIGHT }
                if stick.yAxis.value < -0.35 { physical |= R3_PAD_DOWN }
                if stick.yAxis.value >  0.35 { physical |= R3_PAD_UP }
            }
            if pad.buttonA.isPressed { physical |= R3_PAD_A }
            if pad.buttonB.isPressed { physical |= R3_PAD_B }
            if pad.buttonX.isPressed || pad.buttonY.isPressed { physical |= R3_PAD_C }
            if pad.leftShoulder.isPressed || pad.leftTrigger.isPressed { physical |= R3_PAD_LEFT_SHIFT }
            if pad.rightShoulder.isPressed || pad.rightTrigger.isPressed { physical |= R3_PAD_RIGHT_SHIFT }
            if pad.buttonMenu.isPressed { physical |= R3_PAD_PLAY }
            if pad.buttonOptions?.isPressed == true { physical |= R3_PAD_STOP }
        }
        publish()
    }

    private func publish() { r3_pad(active ? touch | physical : 0) }
}
