# MSVC portability repairs

The third owner archive declares clean `02ce967`. The previous `SceneView`
alignment repair compiles. Ninja now collects the remaining independent failures
without disabling `/W4 /WX`:

- `Mixer::render`: `std::fill` deduces an integer source for float PCM.
- `Flight::step`: repeated if-initializer names shadow earlier declarations in
  the else-if scope.
- `ShipProfile`: implicit alignment bytes before its root matrix and at its tail.
- Presentation: a braced return cannot use Jolt's explicit matrix constructor.
- SDL walkthrough: a button event shadows the button helper.
- Scene tests: integer pair arguments instantiate float conversions in the STL.

Use float literals, distinct intent/event names and direct typed Jolt construction.
Name the profile's existing padding rather than suppressing C4324. Before/after
native layout probes agree: ShipDefinition136, ShipProfile224/alignment16,
flight0/root144/CRC208. No existing member offset, 64-byte material ABI, gain,
control limit, navigation rule or authored asset policy changes. Test values and
assertions retain their meaning. These are compiler failures, not gameplay Reds;
existing public behavior tests are reused rather than adding tautological tests.

Eight native graphs/116 CTests,24 complete actual Clang commands across six
configurations, pinned nonmutating formatting and four330-frame native SDL/GUI/
audio runs pass. All native stderr is empty. Strict ASan/UBSan/leak checks stay
active; traces verify tactical coast, bounded audio and both reset orbits. GCC
uses its native build/tests, not translated Clang flags. Same-session review
finds no further actionable defect; this is not independent review or a native
MSVC/ClangCl success claim. Current Windows builds and RTX acceptance remain pending.

Archive metadata is bounded/preflighted and selected diagnostics pass a redacted
secret scan. Session/bootstrap and successful dependency-build payloads remain
unread/unextracted; no returned code runs. Private asset/media originals remain
untouched. Raw local checks use `build/m3-flight/msvc-portability-*`.
