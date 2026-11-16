# Store screenshots

Empty on purpose.

The eight files that used to sit here were captured from the Retro-3DO build:
they show the old name in the shelf header and the old slate-blue palette, and
they are the same four images attached to App Store record 6808591874, which
was rejected under guideline 4.3(a). Uploading them to the new record would put
a rejected app's screenshots on the page of the app meant to replace it.

They are regenerated, not edited. Push `ios-shelf` and the `screenshots` job in
.github/workflows/ios.yml captures both device families on simulators and
uploads them as the `spryte-screenshots-iphone-6.9in` and
`spryte-screenshots-ipad-13in` artifacts.

Two things that have caught this project before and are worth repeating here:

- The required sizes are 2868x1320 (APP_IPHONE_69, stored as `_67`) and
  2752x2064 (APP_IPAD_PRO_3GEN_129). Get them from `asc screenshots sizes
  --all`, never from the help text, whose examples use a size that will not
  accept 2868x1320.
- simctl captures the physical orientation, and iOS picks a different landscape
  on each device, so the iPhone needs `sips -r 270` and the iPad `sips -r 90`.
  A sideways shot has exactly the right pixel count and passes every automatic
  check, so it has to be looked at by a person.
