import csv
import pathlib
import sys
rows = list(csv.DictReader(pathlib.Path(sys.argv[1]).open(newline='')))
assert rows and int(rows[0]['mode']) == 1, ('first SDL TAB must enter direct mode', rows[0]['mode'] if rows else 'no rows')
assert float(rows[0]['fz_N']) < -71_990_000, ('first SDL W must reach held forward thrust', rows[0]['fz_N'])
assert abs(float(rows[0]['vz_mps']) + .1) < 1e-5
if len(rows) >= 210:
    assert all(abs(float(r['fz_N'])) < 1 for r in rows[30:60]), 'released W must coast'
    assert max(float(r['vz_mps']) for r in rows[30:60]) - min(float(r['vz_mps']) for r in rows[30:60]) < 1e-5
    assert any(float(r['fz_N']) > 1e6 for r in rows[60:90]), 'SDL SPACE must brake'
    # Public braking is bounded proportional opposing force (0.35 s), not an instantaneous stop.
    assert abs(float(rows[89]['vz_mps'])) < 1.0
    assert all(0 < float(r['fz_N']) <= 72_000_001 for r in rows[60:90])
    assert any(abs(float(r['ty_Nm'])) > 1e6 for r in rows[90:120]), 'captured SDL motion must request physical yaw'
    assert all(abs(float(r['fx_N'])) + abs(float(r['fy_N'])) + abs(float(r['fz_N'])) < 1 for r in rows[120:180]), 'focus loss/gain must not resume thrust'
    assert all(int(r['mode']) == 0 for r in rows[180:]), 'second SDL TAB returns to tactical mode'
    assert all(int(r['capture_requested']) == int(r['capture_accepted']) == 1 for r in rows[90:120]), 'SDL must accept actual relative-mode requests'
    assert all(float(r['drive_power']) < 1e-6 for r in rows[90:120]), 'lateral thrust while turning must not ignite the aft main drive'
    assert float(rows[119]['vx_mps']) > .9, 'SDL D must drive the lateral actuator before focus loss'
    assert all(int(r['input_focused']) == 0 for r in rows[120:150])
    assert all(int(r['capture_requested']) == int(r['capture_accepted']) == 0 for r in rows[120:180]), 'focus gain must not recapture a stale pointer'
    assert abs(float(rows[179]['wy_radps'])) < abs(float(rows[149]['wy_radps'])), 'released virtual stick must stabilize, not silently resume turning'
    for index in [120,150,180]:
        delta = sum((float(rows[index][k])-float(rows[index-1][k]))**2 for k in ['x_m','y_m','z_m'])**.5
        speed = sum(float(rows[index][k])**2 for k in ['vx_mps','vy_mps','vz_mps'])**.5
        assert delta <= speed / 60 + 1e-5, ('focus/mode may only advance physical pose through its bounded velocity', index, delta)
if len(rows) >= 270:
    for index in [210,240]:
        eye_delta = sum((float(rows[index][k])-float(rows[index-1][k]))**2 for k in ['camera_x_m','camera_y_m','camera_z_m'])**.5
        assert eye_delta > 100, ('F must toggle target focus and ship follow', index, eye_delta)
        physical_delta = sum((float(rows[index][k])-float(rows[index-1][k]))**2 for k in ['x_m','y_m','z_m'])**.5
        assert physical_delta < 2, 'camera focus may not teleport the physical hull'
if len(rows) >= 330:
    changes = [i for i in range(1,len(rows)) if rows[i]['entity_generation'] != rows[i-1]['entity_generation']]
    assert changes == [270,302] or changes == [270,301], ('keyboard and actual GUI button must each reset once', changes)
    for i,r in enumerate(rows):
        error=max(abs(float(r['render_'+a+'_m'])-float(r[a+'_m'])) for a in ['x','y','z'])
        assert error < 1e-6, ('a reset may not mix rendered and newly reset physical snapshots', i, error)
assert all(int(r['gpu_ready']) == 1 for r in rows)
print(f'walkthrough rows={len(rows)} public SDL-event/held-intent assertions passed')
