# Owner-requested steering trim

After actually trying `8a37eb6`, the owner reported: “Much better but slow it down
by like5–10%.” The authored maximum rate is trimmed7.5%, from0.4 to0.37rad/s
(about21.2degrees/s). Torque limits and0.1s angular-rate response stay unchanged,
so release braking is not deliberately slowed. Linear braking remains untouched.

Four21-test demo graphs and three complete profile Clang commands pass; the two
public response/arrest cases still pass on all four compilers/configurations.
One current330-frame native Release validation run passes control/audio checks,
including both resets, with empty stderr. Earlier headless/control implementation
verification remains scoped to its unchanged sources. Final owner confirmation,
current benchmarks and Windows still remain.

Mute is now physically confirmed by the owner. Listening sounded good but quiet;
real focus/reconnection has not been confirmed. The first recording attempt did
not show Spectacle or create a video. Its original application-only PCM WAV remains
private; audio alone is not the required gameplay clip. The installed FFmpeg lacks
a PipeWire input and the GStreamer PipeWire bridge is absent. No privileged whole-
display capture or microphone recording was substituted.
