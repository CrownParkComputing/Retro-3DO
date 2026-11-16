import SwiftUI

// Spryte's own look, and deliberately not any other app's.
//
// This matters for more than taste. The App Store rejected this app and its
// siblings under guideline 4.3 because they shared one interface; the answer is
// not a new icon on the same screens, it is a different shape and a different
// palette per app. DOSDeck is amber phosphor on near-black and is built around
// a question. This is plum and spectrum and is built around a shelf: one
// screen, tiles, tap to play.
//
// Plum, where this was slate-blue until the app was renamed. Two reasons. It
// agrees with the icon, which is a spectrum quad on a plum ground
// (tools/make-spryte-icon.py). And the slate-blue shelf is what is sitting on
// the rejected Retro-3DO record as four screenshots: a new app whose store
// page shows the same screens as a rejected one is the comparison 4.3(a) is
// looking for, and the shelf is the first thing it shows.
enum Theme {
    // The machine was a black box under a television. The ground is that black
    // lifted toward the plum the icon sits on, never pure #000 — flat black
    // makes the tiles look pasted on rather than lit.
    static let ground      = Color(red: 0.102, green: 0.051, blue: 0.153)
    static let groundLow   = Color(red: 0.039, green: 0.020, blue: 0.071)
    static let tile        = Color(red: 0.169, green: 0.102, blue: 0.239)
    static let tileEdge    = Color(red: 0.278, green: 0.192, blue: 0.376)

    static let ink         = Color(red: 0.961, green: 0.937, blue: 0.996)
    static let inkDim      = Color(red: 0.690, green: 0.612, blue: 0.761)

    // The 3DO's own mark was a spectrum sweep. It is used as a sweep — on the
    // demo tile and the add tile — and never as flat fill, because three strong
    // hues behind text is unreadable.
    static let spectrum = LinearGradient(
        colors: [Color(red: 0.98, green: 0.31, blue: 0.47),
                 Color(red: 0.99, green: 0.62, blue: 0.24),
                 Color(red: 0.31, green: 0.82, blue: 0.91)],
        startPoint: .topLeading, endPoint: .bottomTrailing)

    static let accent      = Color(red: 0.99, green: 0.62, blue: 0.24)

    static let background = LinearGradient(
        colors: [ground, groundLow],
        startPoint: .top, endPoint: .bottom)

    static let tileCorner: CGFloat = 18
}
