import io
import re

p = r"C:\Users\Administrator\Documents\Codex\2026-11-19\dui\work\H2O-firmware-onenet\web\dashboard.html"
s = io.open(p, encoding="utf-8").read()


def rep(old, new):
    global s
    assert old in s, "MISS: " + old[:60]
    s = s.replace(old, new, 1)


# 1) 换库：echarts(1MB) -> chart.js(205KB / 69KB gzip)，同源自托管
rep('<script src="https://cdn.jsdelivr.net/npm/echarts@5.5.1/dist/echarts.min.js"></script>',
    '<script defer src="/h2o/lib/chart.umd.min.js"></script>')

# 2) 图表容器
rep("  #chart{width:100%;height:250px}",
    "  .chart-wrap{position:relative;height:250px}\n  #chart{width:100%;height:100%}")
rep('<div id="chart"></div>', '<div class="chart-wrap"><canvas id="chart"></canvas></div>')

# 3) initChart 整体替换（从 function initChart 到其结束）
i = s.index("function initChart(){")
j = s.index("\n}\n", s.index("addEventListener('resize'", i)) + 3
new_init = """function initChart(){
  const ctx = $('chart').getContext('2d');
  chart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        {label:'温度', data:[], borderColor:'#4d6bfe', backgroundColor:'rgba(77,107,254,.10)',
         borderWidth:2.5, tension:.35, pointRadius:0, fill:true, yAxisID:'y'},
        {label:'湿度', data:[], borderColor:'#10b981', backgroundColor:'rgba(16,185,129,.10)',
         borderWidth:2.5, tension:.35, pointRadius:0, fill:true, yAxisID:'y'},
        {label:'烟雾', data:[], borderColor:'#f59e0b', borderWidth:2.5, tension:.35,
         pointRadius:0, fill:false, yAxisID:'y1'}
      ]
    },
    options: {
      responsive:true, maintainAspectRatio:false, animation:false,
      interaction:{mode:'index', intersect:false},
      plugins:{
        legend:{labels:{color:'#6b7891', boxWidth:10, boxHeight:8, font:{size:11}}},
        tooltip:{backgroundColor:'#fff', borderColor:'#e4e9f2', borderWidth:1,
                 titleColor:'#1b2333', bodyColor:'#1b2333', padding:10, cornerRadius:10}
      },
      scales:{
        x:{display:false, grid:{display:false}},
        y:{position:'left', grid:{color:'#eef2f8'}, border:{display:false},
           ticks:{color:'#6b7891', font:{size:10}, maxTicksLimit:5}},
        y1:{position:'right', display:false, grid:{display:false}}
      }
    }
  });
}
"""
s = s[:i] + new_init + s[j:]

# 4) 两处 chart.setOption({...xAxis...series...}) 统一换成 Chart.js 的更新
pattern = re.compile(r"[ \t]*chart\.setOption\(\{\s*xAxis:\{data:times\},.*?\}\);", re.S)
repl = ("chart.data.labels = times;\n"
        "  chart.data.datasets[0].data = series.temperature;\n"
        "  chart.data.datasets[1].data = series.humidity;\n"
        "  chart.data.datasets[2].data = series.smoke;\n"
        "  chart.update('none');")
s, n = pattern.subn(repl, s)
assert n == 2, f"setOption 替换了 {n} 处"

# 5) 首屏历史点数减半
rep("const MAXP = 300;", "const MAXP = 180;")

io.open(p, "w", encoding="utf-8").write(s)
print("ok, len =", len(s), "替换 setOption:", n)
