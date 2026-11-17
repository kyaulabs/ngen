from pathlib import Path
import re,sys
text=Path(sys.argv[1]).read_text()
assert 'audio_pending=0' in text, 'playable application must integrate the real bounded audio owner'
assert 'audio_state=muted' in text, 'explicit CLI mute must reach a paused native stream'
match=re.search(r'audio_gen=(\d+).*audio_losses=(\d+).*audio_queue_max=(\d+)',text)
assert match, 'native audio lifetime/queue telemetry missing'
generation,losses,queued=map(int,match.groups())
assert queued<=4096
if len(sys.argv)>2:
 assert generation>=2 and losses>=1, 'reported device removal/addition must really retire and reopen the owned SDL stream'
else:
 assert generation==1
print('native audio bridge, mute, lifetime and bounded queue assertions passed')
