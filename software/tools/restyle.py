import io

p = r"C:\Users\Administrator\Documents\Codex\2026-11-19\dui\work\H2O-firmware-onenet\web\dashboard.html"
s = io.open(p, encoding="utf-8").read()


def rep(old, new):
    global s
    assert old in s, "MISS: " + old[:60]
    s = s.replace(old, new, 1)


rep(
    """    --bg:#0b1016; --panel:#131b24; --panel2:#18222e; --line:#233040;
    --text:#e6edf3; --dim:#8b9bad; --accent:#2fd6a8; --accent2:#3aa0ff;
    --warn:#ffb454; --danger:#ff5c6c;""",
    """    --bg:#f7f9fc; --panel:#ffffff; --panel2:#ffffff; --line:#e4e9f2;
    --text:#1b2333; --dim:#6b7891; --accent:#10b981; --accent2:#4d6bfe;
    --warn:#f59e0b; --danger:#ef4444; --soft:#eef3ff;""",
)

rep(
    """  body{margin:0;background:radial-gradient(1200px 600px at 20% -10%,#16222e 0%,var(--bg) 55%);
       color:var(--text);font:14px/1.5 "Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;
       min-height:100vh;padding:18px}""",
    """  body{margin:0;color:var(--text);
       background:
         radial-gradient(900px 420px at 12% -8%, #e8efff 0%, rgba(232,239,255,0) 60%),
         radial-gradient(700px 360px at 92% 0%, #e6f8f1 0%, rgba(230,248,241,0) 55%),
         var(--bg);
       font:14px/1.5 "Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;
       min-height:100vh;padding:18px}""",
)

rep(
    """  .badge{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;
         background:#1a2430;border:1px solid var(--line);font-size:12px;color:var(--dim)}""",
    """  .badge{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;
         background:#fff;border:1px solid var(--line);font-size:12px;color:var(--dim);
         box-shadow:0 1px 2px rgba(27,35,51,.04)}""",
)

rep(
    """  button{cursor:pointer;border:1px solid var(--line);background:var(--panel2);color:var(--text);
         padding:7px 14px;border-radius:8px;font-size:13px;transition:.15s}""",
    """  button{cursor:pointer;border:1px solid var(--line);background:#fff;color:var(--text);
         padding:7px 14px;border-radius:8px;font-size:13px;transition:.15s}""",
)

rep(
    "button.primary{background:linear-gradient(135deg,#1f8f74,#2fd6a8);border:0;color:#04140f;font-weight:600}",
    "button.primary{background:linear-gradient(135deg,#4d6bfe,#6b86ff);border:0;color:#fff;font-weight:600;\n                  box-shadow:0 4px 12px rgba(77,107,254,.25)}",
)

rep(
    """  .card{background:linear-gradient(180deg,var(--panel2),var(--panel));border:1px solid var(--line);
        border-radius:12px;padding:14px 16px;position:relative;overflow:hidden}
  .card::after{content:"";position:absolute;inset:0 0 auto 0;height:2px;
        background:linear-gradient(90deg,transparent,var(--accent),transparent);opacity:.5}""",
    """  .card{background:#fff;border:1px solid var(--line);border-radius:14px;padding:14px 16px;
        position:relative;overflow:hidden;box-shadow:0 2px 10px rgba(27,35,51,.05)}
  .card::after{content:"";position:absolute;inset:0 0 auto 0;height:3px;
        background:linear-gradient(90deg,rgba(77,107,254,0),var(--accent2),var(--accent))}""",
)

rep(
    """  .box{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px 16px}""",
    """  .box{background:#fff;border:1px solid var(--line);border-radius:14px;padding:14px 16px;
       box-shadow:0 2px 10px rgba(27,35,51,.05)}""",
)

rep(
    """  .cfg input{width:100%;background:#0e1620;border:1px solid var(--line);color:var(--text);
             padding:8px 10px;border-radius:8px;font-size:13px}""",
    """  .cfg input{width:100%;background:#fbfcfe;border:1px solid var(--line);color:var(--text);
             padding:8px 10px;border-radius:8px;font-size:13px}""",
)

rep(
    "  .card.warn .v{color:var(--warn)} .card.bad .v{color:var(--danger)} .card.ok .v{color:var(--accent)}",
    "  .card .v{color:var(--accent2)}\n"
    "  .card.warn .v{color:var(--warn)} .card.bad .v{color:var(--danger)} .card.ok .v{color:var(--accent)}",
)

rep(
    """  .log{max-height:120px;overflow:auto;font:12px/1.6 ui-monospace,Consolas,monospace;color:var(--dim)}""",
    """  .log{max-height:120px;overflow:auto;font:12px/1.6 ui-monospace,Consolas,monospace;color:var(--dim);
       background:#fbfcfe;border:1px solid var(--line);border-radius:10px;padding:10px 12px}""",
)
rep(".log b{color:var(--accent)}", ".log b{color:var(--accent2)}")

rep(
    """    tooltip:{trigger:'axis',backgroundColor:'#131b24',borderColor:'#233040',
             textStyle:{color:'#e6edf3',fontSize:12}},
    legend:{data:['温度','湿度','烟雾'],textStyle:{color:'#8b9bad',fontSize:11},""",
    """    tooltip:{trigger:'axis',backgroundColor:'#fff',borderColor:'#e4e9f2',
             textStyle:{color:'#1b2333',fontSize:12},
             extraCssText:'box-shadow:0 6px 20px rgba(27,35,51,.12);border-radius:10px'},
    legend:{data:['温度','湿度','烟雾'],textStyle:{color:'#6b7891',fontSize:11},""",
)

rep(
    """    yAxis:[{type:'value',axisLine:{lineStyle:{color:'#233040'}},
            splitLine:{lineStyle:{color:'#1a2430'}},axisLabel:{color:'#8b9bad',fontSize:10}},""",
    """    yAxis:[{type:'value',axisLine:{lineStyle:{color:'#e4e9f2'}},
            splitLine:{lineStyle:{color:'#eef2f8'}},axisLabel:{color:'#6b7891',fontSize:10}},""",
)

rep(
    """      {name:'温度',type:'line',smooth:true,symbol:'none',data:[],
       lineStyle:{width:2,color:'#2fd6a8'},areaStyle:{color:'rgba(47,214,168,.12)'}},
      {name:'湿度',type:'line',smooth:true,symbol:'none',data:[],
       lineStyle:{width:2,color:'#3aa0ff'},areaStyle:{color:'rgba(58,160,255,.10)'}},
      {name:'烟雾',type:'line',smooth:true,symbol:'none',yAxisIndex:1,data:[],
       lineStyle:{width:2,color:'#ffb454'}}""",
    """      {name:'温度',type:'line',smooth:true,symbol:'none',data:[],
       lineStyle:{width:2.5,color:'#4d6bfe'},areaStyle:{color:'rgba(77,107,254,.10)'}},
      {name:'湿度',type:'line',smooth:true,symbol:'none',data:[],
       lineStyle:{width:2.5,color:'#10b981'},areaStyle:{color:'rgba(16,185,129,.10)'}},
      {name:'烟雾',type:'line',smooth:true,symbol:'none',yAxisIndex:1,data:[],
       lineStyle:{width:2.5,color:'#f59e0b'}}""",
)

io.open(p, "w", encoding="utf-8").write(s)
print("light theme applied")
