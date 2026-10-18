import csv,json,math,sys
from pathlib import Path
def pct(values,p): return sorted(values)[math.ceil(len(values)*p)-1]
out=[]
for name in sys.argv[1:]:
    groups={}
    for line in Path(name).read_text().splitlines():
        if not line.startswith(('TRACE,','RENDER,')):continue
        row=line.split(','); key=tuple(row[:4]); frame=int(row[4])
        if frame<4:continue
        values=list(map(float,row[5:]));groups.setdefault(key,[]).append((frame,values))
    entries=[]
    for key,rows in groups.items():
        render=key[0]=='RENDER'; n=3 if render else 8
        total=[sum(v[:n]) for f,v in rows]
        labels=['acquire_api','draw','end_api','fence','vk_acquire','vk_submit','vk_present','images','mode'] if render else ['reload','acquire_api','populate','index','query','extract','draw','end_api','fence','vk_acquire','vk_submit','vk_present','image','images','requested_images','mode']
        entries.append(dict(case=key,frames=len(rows),p95=pct(total,.95),p99=pct(total,.99),maximum=max(total),stage_p95={k:pct([v[j] for f,v in rows],.95) for j,k in enumerate(labels)},slowest=[dict(frame=f,total=sum(v[:n]),**dict(zip(labels,v))) for f,v in sorted(rows,key=lambda r:sum(r[1][:n]),reverse=True)[:10]]))
    result=dict(file=name,cases=entries);out.append(result)
    print(json.dumps(result,indent=2))
Path(sys.argv[1]+'.summary.json').write_text(json.dumps(out,indent=2)+'\n')
