# Physical steering feedback and response repair

The owner actually tested the interactive client: translational braking worked
perfectly; RMB steering had a low maximum turn rate and continued turning too long
**after releasing RMB**. This was not a complaint about axes or stopping hand
motion while RMB remained held. Listening was good but quiet; mute subsequently
passed. Focus/reconnection and original audio/video recording remain open.

The authored frigate previously allowed0.15rad/s (8.6degrees/s) and approximately
0.06rad/s² torque-limited angular acceleration. Its large box inertia made release
arrest take seconds. The profile now allows0.4rad/s (22.9degrees/s) and approximately
1.6rad/s² through explicit per-axis torque limits below the existing admission cap.
The common angular-rate response interval changes from0.25s to0.1s; all commanded
acceleration still passes the same mass/inertia-aware bounded torque controller.
Linear thrust,0.35s translational braking, mouse mapping, pose/velocity ownership,
assets and camera mounts are unchanged. No angular velocity is assigned or reset.

Two public application-profile regressions fail before the corresponding repairs:
all three axes must reach at least0.35rad/s within one second, and actual platform-
neutral RMB release must reduce pitch/yaw below0.04rad/s within0.3s. The latter also
requires nonzero first-tick rotation, bounded torque, unchanged entity generation
and no injected translation. The scene executable now has10 cases.

Four21-test demo graphs and four8-test headless graphs pass. Twelve complete native
Clang invocations cover changed implementations/profile tests in their actual demo
and headless configurations. The initial selector failed on a vendor TU path before
analysis; exact source-path selection fixes that setup error without removing flags.
Four330-frame native SDL/GUI/audio correctness walkthroughs pass, with empty stderr,
all GPU rows, real relative-mode acceptance, two resets, mute and reported recovery.
They use dummy playback and do not replace physical feedback or current timing.

Same-session review traced profile inertia/limits, shared rate control, input release
and unchanged linear braking. The response is deliberately finite, not FPS camera
teleportation. The owner must retest feel before acceptance; latest-source laptop,
Windows/RTX, reconnect, recording and reviewed integration remain required.

Recording assistance also exercised an application-only ALSA file-PCM playback tee
using child-scoped configuration and existing platform tools. It captures no microphone
or other application audio and requires no application API/dependency. Its first SDK
attempt had the wrong ALSA configuration root; explicit platform paths produce a
valid stereo48kHz PCM WAV. That short smoke is not the required gameplay clip.
