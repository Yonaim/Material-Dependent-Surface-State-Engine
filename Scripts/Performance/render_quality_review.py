"""Create traceable contact sheets of captured frames for the study's visual review."""
from pathlib import Path
import datetime,json,sys
from analyze_suite import load,collect,image_pixels
import numpy as np
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[2]
RESULTS=ROOT/'Docs/MDSSP-Vault/04_Development/Performance-Results'
MAIN=RESULTS/'20261022-optimization-suite-main'
EXTRA=RESULTS/'20261022-workload-supplement'
OUTPUT=MAIN/'raw/quality-review'
OUTPUT.mkdir(exist_ok=True)
FONT=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',17)
manifests={base:load(base/'manifest.json') for base in [MAIN,EXTRA]}


def capture(base,group,scene,resolution,variant):
    run=next(r for r in manifests[base]['runs'] if r['group']==group and r['scene_name']==scene and r['resolution']==resolution and r['variant']==variant and r['repeat']==1)
    item=collect(base,run)
    assert item['validation']['simulation_step_serial']==662
    return image_pixels(base,item),item['validation']['instances'],run['id']


records=[]
for scene in ['BrickCube','Mountain']:
    reference= capture(MAIN,'G7',scene,512,'reference')
    coarse128=capture(MAIN,'G7',scene,512,'mesh128')
    coarse64= capture(EXTRA,'G7',scene,512,'mesh64')
    assert reference[1]==coarse128[1]==coarse64[1], 'Simulation state differs in the LOD figure'
    images=[reference,coarse128,coarse64]
    width,height=reference[0].shape[1],reference[0].shape[0]
    pad,title=14,34
    sheet=Image.new('RGB',(3*width+4*pad,2*(height+title)+3*pad),(24,27,31));draw=ImageDraw.Draw(sheet)
    for column,(pixels,state,run_id) in enumerate(images):
        x=pad+column*(width+pad)
        label=['Render mesh 512 (reference)','Render mesh 128','Render mesh 64'][column]
        draw.text((x,pad+7),label,font=FONT,fill='white')
        sheet.paste(Image.fromarray(pixels),(x,pad+title))
        diff=np.clip(np.abs(pixels.astype(np.int16)-reference[0].astype(np.int16))*4,0,255).astype(np.uint8)
        y=2*pad+height+title
        draw.text((x,y+7),'Absolute RGB difference x4',font=FONT,fill='white')
        sheet.paste(Image.fromarray(diff),(x,y+title))
    filename=f'{scene.lower()}-512-lod.png';sheet.save(OUTPUT/filename)
    records.append({'image':filename,'scene':scene,'simulation_resolution':512,'simulation_steps':662,
                    'run_ids':[i[2] for i in images],'state_records_equal':True,'difference_amplification':4})

reference=capture(MAIN,'G6','BrickCube',256,'reference')
fragment=capture(MAIN,'G6','BrickCube',256,'fragment_texture')
assert reference[1]==fragment[1], 'Simulation state differs in the filter figure'
pixels=[reference[0],fragment[0],np.clip(np.abs(reference[0].astype(np.int16)-fragment[0].astype(np.int16))*16,0,255).astype(np.uint8)]
height,width=pixels[0].shape[:2];sheet=Image.new('RGB',(3*width+56,height+62),(24,27,31));draw=ImageDraw.Draw(sheet)
for i,pixel in enumerate(pixels):
    x=14+i*(width+14)
    draw.text((x,21),['Precompute ON','Fragment filter (texture)','Absolute RGB difference x16'][i],font=FONT,fill='white')
    sheet.paste(Image.fromarray(pixel),(x,48))
sheet.save(OUTPUT/'brickcube-256-filter.png')
records.append({'image':'brickcube-256-filter.png','scene':'BrickCube','simulation_resolution':256,'simulation_steps':662,
                'run_ids':[reference[2],fragment[2]],'state_records_equal':True,'difference_amplification':16})
(OUTPUT/'review-inputs.json').write_text(json.dumps({'generated_at':datetime.datetime.now().astimezone().isoformat(),
    'scope':'UI-free final captures cropped using saved warmed viewport. Original pixels preserved; only difference panels are amplified. No loading arms included.',
    'visual_review_status':'pending','figures':records},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('Created three visual review sheets:',OUTPUT)
