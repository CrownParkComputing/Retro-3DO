import SwiftUI

// Everything that is not playing a game, on one sheet, in plain words.
//
// What is deliberately absent: the renderer picker, the Vulkan and GPU-driver
// controls, the CPU scaling, the bezel and CRT toggles, the free-text BIOS and
// disc path fields and the file browser. Those were the old System page. None
// of them is a thing a person who wants to play a game needs to decide.
struct SettingsSheet: View {
    var showGuide: () -> Void
    @EnvironmentObject private var engine: Engine
    @Environment(\.dismiss) private var dismiss
    @State private var pal = false
    @State private var editingControls = false

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Button(action: showGuide) {
                        Label("How Spryte works", systemImage: "book")
                    }
                } footer: {
                    Text("The setup guide from your first launch. It is here whenever you want it.")
                }

                Section {
                    Button("Arrange my controls") { editingControls = true }
                } header: {
                    Text("On-screen controls")
                } footer: {
                    Text("Available even with a controller connected. Your layout is remembered.")
                }

                Section {
                    Picker("Television", selection: $pal) {
                        Text("60 Hz (most games)").tag(false)
                        Text("50 Hz (European discs)").tag(true)
                    }
                    .pickerStyle(.inline)
                    .labelsHidden()
                } header: {
                    Text("Speed")
                } footer: {
                    Text("Only change this if a disc runs too fast or too slow.")
                }

                Section {
                    LabeledContent("System ROM",
                                   value: engine.hasBios ? "Added" : "Not added yet")
                } footer: {
                    Text(engine.hasBios
                         ? "Your discs will play."
                         : "The demo plays without one. Your own discs need a 3DO system ROM — add it with the + tile, the same way you add a disc.")
                }

                Section("About") {
                    LabeledContent("Emulator", value: "Spryte")
                    Link("Crown Park Computing",
                         destination: URL(string: "https://www.crownparkcomputing.com/")!)
                }
            }
            .navigationTitle("Settings")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .onAppear { pal = engine.isPal }
        .onChange(of: pal) { engine.isPal = $0 }
        .fullScreenCover(isPresented: $editingControls) { PadLayoutEditor() }
    }
}
