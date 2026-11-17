from pathlib import Path
import csv,hashlib,json,math,re,struct,subprocess
if not __debug__:raise SystemExit('Verification requires assertions; do not use python -O.')
root=Path.cwd();p=root/'build/windows-rtx-lattice-fix-owner-1';revision='c6b82dac08c41f9b73b92cad7ae6b00e569a40fc'
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def text(name):return (p/name).read_text(encoding='utf-8-sig')
assert text('revision.txt').strip()==revision and not text('working-tree.txt').strip()
assert not subprocess.check_output(['git','diff','--name-only',revision,'HEAD','--','apps','engine','shaders','tools/build-windows.ps1'],text=True).strip()
assert '100% tests passed out of 10' in text('Release-tests.txt') and len(re.findall(r'Test\s+#\d+:.*Passed',text('Release-tests.txt')))==10 and 'Total Tests: 10' in text('Release-test-list.txt')
config=text('Release-configure.txt');assert 'MSVC 19.44.35229.0' in config and 'C++ Standard 20 with extensions OFF' in config and 'MultiThreadedDLL' in config
build=text('Release-build.txt');assert '[153/154]' in build
for name in ('resource_probe.exe','inspector_probe.exe','scene_probe.exe','scene_workload.exe'):assert 'Linking CXX executable '+name in build
resource=text('Release-resources.txt')
for key in ('continuity_calibration','admission','tail_motion','aft','core_anchored','wrap','disabled','soft_tip','irregular_streaks','seed','ragged_skin','bounded_uv'):assert f'emissive_flow_{key}=1' in resource
assert 'Vulkan resource:' not in resource and 'buffer_lifetime=1' in resource and 'close_invalidated=1' in resource
inspector=text('Release-inspector.txt')
for key in ('scene_emissive_flow','scene_flow_copied','scene_flow_disabled','scene_translucent_order','scene_opaque_occlusion','scene_translucent_depth_readonly','inspector_ship_bounds','inspector_rear_engine_bounds'):assert key+'=1' in inspector
package=json.loads(text('Release-scene-package.json'));assert package['sha256']=='a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed' and package['bytes']==194096492
scene=text('Release-scene-package.txt')
for marker in ('resident_images=9','scene_materials=17','scene_triangles=239542','scene_draws=210','resident_verified_mip_bytes=106343076','resident_vertex_readback=1','resident_index_readback=1','resident_mip_readback=1'):assert marker in scene
manifest=json.loads(text('Release-shader-manifest.json'));assert manifest['schema']==1 and manifest['compiler']['version']=='2026.21' and len(manifest['shaders'])==9
for shader in manifest['shaders']:
 name=shader['name'];assert sha(p/f'Release-{name}.reflection.json')==shader['reflection_sha256']
 source='pbr.slang' if name.startswith(('pbr-','shadow-')) else ('cull.slang' if name=='cull' else ('post.slang' if name.startswith('post-') else 'overlay.slang'))
 assert sha(root/'shaders'/source)==shader['source_sha256'],(name,'shader source identity mismatch')
assert sha(p/'Release-shader-manifest.json')!=sha(root/'docs/evidence/M2/closure/windows-final/ui-5a2/Release-shader-manifest.json')
with (p/'Release-steady.csv').open(newline='',encoding='utf-8-sig') as f:steady_rows=list(csv.DictReader(f))
baseline={k:int(steady_rows[-1][k]) for k in ('vma_allocation_bytes','vma_reserved_bytes','resource_live','resource_retiring')}
results={}
for mode,n,reloads,resizes,validation in [('inspection',1200,2,2,1),('steady',600,0,0,0),('stream',600,4,0,0)]:
 stem='Release-'+mode;log=text(stem+'.txt');assert (p/(stem+'.txt.stderr.txt')).stat().st_size==0
 with (p/(stem+'.csv')).open(newline='',encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
 assert len(rows)==n and len(rows[0])==19 and [int(r['frame']) for r in rows]==list(range(n));assert all(math.isfinite(float(v)) and float(v)>=0 for r in rows for v in r.values())
 phases=[float(r['emissive_flow_phase']) for r in rows];assert all(0<=v<1 for v in phases) and len(set(phases))>=n*.9 and all(0<(b-a)%1<.1 for a,b in zip(phases,phases[1:]));assert 'emissive_flow_enabled=1 flow_material=13 flow_clock=steady flow_cycle_seconds=16' in log
 hosts=[int(r['host_ns']) for r in rows];assert all(a<b for a,b in zip(hosts,hosts[1:]));distributions={}
 for col in ('frame_ns','prepare_ns','present_ns','gpu_shadow_ns','gpu_pbr_ns','gpu_post_ns','ui_prepare_ns','gpu_ui_ns'):
  values=sorted(float(r[col]) for r in rows);distributions[col]={f'p{q}':values[(n*q+99)//100-1] for q in (50,95,99)};distributions[col]['max']=values[-1]
  if col.startswith('gpu_'):assert values[0]>0
 timing=distributions['frame_ns'];line=next(l for l in log.splitlines() if l.startswith('frame_ns '))
 for key,value in timing.items():assert int(value)==int(re.search(rf'\b{key}=(\d+)',line)[1])
 if not validation:assert timing['p50']<=50e6 and timing['p95']<=66.7e6 and timing['p99']<=100e6 and timing['max']<=200e6
 assert 'ship_triangles=239542 total_triangles=247542 scene_draws=211' in log and 'fixture_crc32=3f1c5f23 package_crc32=a2eb7c22' in log
 assert f'workload_frames={n} workload_warmup=60 workload_wsi_queued={n} workload_hdr_finite=1' in log and f'shadow_extent=1024 validation={validation}' in log and 'post_exposure=1.00 fxaa=1 bloom_strength=0.0400' in log
 extents={};changes=[];generations=[]
 for i,r in enumerate(rows):
  extent=[int(r['surface_width']),int(r['surface_height'])];key=f'{extent[0]}x{extent[1]}';extents[key]=extents.get(key,0)+1
  if not changes or changes[-1]['extent']!=extent:changes.append({'frame':i,'extent':extent})
  gen=int(r['generation'])
  if not generations or generations[-1]['generation']!=gen:generations.append({'frame':i,'generation':gen})
 assert len(changes)==1+resizes and [g['generation'] for g in generations]==list(range(1,reloads+2))
 if resizes:assert [c['extent'] for c in changes]==[[1280,720],[1024,640],[1280,720]]
 else:assert set(extents)=={'1280x720'}
 assert f'inspector_snapshot_generation={1+reloads}' in log
 if reloads:assert f'inspector_resizes={resizes} inspector_reloads={reloads}' in log and 'inspector_reload_phase=idle' in log
 else:assert 'warm_cpp_allocations=0' in log and 'warm_imgui_allocations=0' in log
 tail=rows[-300:];tracked={}
 # Inspect the last300 without pretending all are idle. Async reload duration is
 # wall-clock work: the fast streaming run publishes generation5 at frame352.
 # Verify every row of the complete final-generation interval against steady
 # baseline, not a fixed suffix that can legitimately include active reloads.
 final_start=generations[-1]['frame'];settled=rows[final_start:]
 assert settled and all(int(r['reload_active'])==0 and all(int(r[k])==v for k,v in baseline.items()) for r in settled)
 settlement={'final_generation':generations[-1]['generation'],'first_frame':final_start,'frames':len(settled),'every_final_generation_row_matches_steady_baseline':True,'baseline':baseline,'last300_includes_active_reload':any(int(r['reload_active'])!=0 for r in tail)}
 for col in ('vma_allocation_bytes','vma_reserved_bytes','resource_live','resource_retiring'):
  values=[int(r[col]) for r in rows];settled=[int(r[col]) for r in tail];tracked[col]={'peak':max(values),'final':values[-1],'last300_min':min(settled),'last300_max':max(settled)}
 assert tracked['vma_allocation_bytes']['peak']<=512*1024**2
 memory=json.loads(text(stem+'.working-set.json'));assert memory['exit_code']==0 and not memory['timed_out'] and memory['elapsed_ms']<180000 and max(memory['observed_peak_working_set_bytes'],memory['observed_peak_counter_bytes'])<=6*1024**3
 artifacts=json.loads(text(stem+'.artifacts.json'));assert len(artifacts)==2 and {a['name'] for a in artifacts}=={stem+'.bmp',stem+'.csv'}
 for artifact in artifacts:assert (p/artifact['name']).stat().st_size==artifact['bytes'] and sha(p/artifact['name'])==artifact['sha256']
 bmp=(p/(stem+'.bmp')).read_bytes();assert bmp[:2]==b'BM';off=struct.unpack_from('<I',bmp,10)[0];w,h=struct.unpack_from('<ii',bmp,18);assert (w,h)==(1280,720) and struct.unpack_from('<H',bmp,28)[0]==32
 colored=[x for x in range(w) if any(bmp[off+((h-1-100)*w+x)*4:off+((h-1-100)*w+x)*4+3])];assert colored[0]==16 and w-1-colored[-1]==16
 results[mode]={'measured_frames':n,'warmup_frames':60,'validation':bool(validation),'phase_unique':len(set(phases)),'phase_domain_and_progression_checked':True,'distributions_ns':distributions,'extent_transitions':changes,'extent_counts':extents,'generation_transitions':generations,'tracked':tracked,'settlement':settlement,'observed_working_set':memory,'private_capture_sha256':sha(p/(stem+'.bmp')),'private_capture_extent':[w,h],'capture_gutters':[16,16],'optimized_timing':not bool(validation),'settings_scope':'Every frame counted; walkthrough1-2 exposure.75/FXAAoff, restored3' if reloads else 'Default exposure1/FXAAon/bloom.04 throughout'}
output={'archive_sha256':'b5a91d23d43f6a1c3eed9e6d4ed9419f3728cfc4f01edbaea38fdf5fcce18649','revision':revision,'reported_working_tree_clean':True,'native_ctests':10,'native_flow_calibration_and_contracts_pass':True,'scene_copied_flow_and_disabled_pass':True,'package':package,'shader_manifest_sha256':sha(p/'Release-shader-manifest.json'),'all_nine_shader_sources_and_reflections_match':True,'shader_identities':manifest['shaders'],'runs':results,'all_measured_rows':sum(r['measured_frames'] for r in results.values()),'binary_scope':'Fresh graph links/executes owned resource/inspector/scene/workload executables with exact reported clean source and new calibration/copy/phase markers. No executable bytes or standalone executable SHA was returned; do not invent one.','memory_scope':'Observed OS working set/high-water counters and tracked VMA only. Prior WDDM evidence remains separate; no new continuous/global/GPU process peak claim.','acceptance_scope':'Current-source Windows flow/final-package physical execution gate demonstrated. Normal reviewed PR integration and post-merge verification remain before M2 closure/M3.'}
Path('build/m2-close-check/rtx-postfix-analysis.json').write_text(json.dumps(output,indent=2)+'\n');print(json.dumps({'revision':revision,'ctests':10,'native_flow':True,'rows':output['all_measured_rows'],'frame_ns':{mode:r['distributions_ns']['frame_ns'] for mode,r in results.items()},'observed_working_set_peak':{mode:r['observed_working_set']['observed_peak_working_set_bytes'] for mode,r in results.items()}},indent=2))
