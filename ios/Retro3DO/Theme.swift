import SwiftUI

// Retro-3DO's own look, and deliberately not any other app's.
//
// This matters for more than taste. The App Store rejected this app and its
// siblings under guideline 4.3 because they shared one interface; the answer is
// not a new icon on the same screens, it is a different shape and a different
// palette per app. DOSDeck is amber phosphor on near-black and is built around
// a question. This is slate and spectrum and is built around a shelf: one
// screen, tiles, tap to play.
enum Theme {
    // The machine was a black box under a television. The ground is that black
    // with the blue of a CRT in a dark room, never pure #000 — flat black makes
    // the tiles look pasted on rather than lit.
    static let ground      = Color(red: 0.043, green: 0.063, blue: 0.125)
    static let groundLow   = Color(red: 0.020, green: 0.031, blue: 0.071)
    static let tile        = Color(red: 0.102, green: 0.133, blue: 0.220)
    static let tileEdge    = Color(red: 0.180, green: 0.227, blue: 0.345)

    static let ink         = Color(red: 0.925, green: 0.945, blue: 0.996)
    static let inkDim      = Color(red: 0.569, green: 0.624, blue: 0.753)

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
