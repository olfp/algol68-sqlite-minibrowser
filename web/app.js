/* Gorgona DB Browser fuer Gorgona (Unterprojekt in web/).
   Endlos-Scrollen: Es wird immer nur ein Fenster von Zeilen gehalten
   (max. MAX = 3 x 50 Zeilen). Scrollt man ans Fensterende, wird automatisch
   nachgeladen; nicht sichtbare Zeilen werden verworfen und beim
   Hoch-/Runterscrollen bei Bedarf neu geladen. Zum Bloecken mit Zeilen-
   Abrufen liefert der Server pro Table-Route "total" und versteht
   ?offset=&count=. Die Seite wird als echte Datei von Disk geliefert,
   daher gelten hier keine ga68-String-Beschraenkungen. */
(function () {
  "use strict";
  var sel = document.getElementById("sel"),
      filt = document.getElementById("f"),
      thead = document.getElementById("th"),
      tbody = document.getElementById("tb"),
      box = document.querySelector(".box"),
      status = document.getElementById("status"),
      sub = document.getElementById("sub"),
      overlay = document.getElementById("overlay"),
      dlgForm = document.getElementById("dlgForm"),
      erBtn = document.getElementById("erBtn"),
      erClose = document.getElementById("erClose"),
      er = document.getElementById("er");

  var BATCH = 50,              /* Zeilen pro Request (Server-Default) */
      MAX = 3 * BATCH,         /* Limit der geladenen Zeilen */
      MARGIN = BATCH;          /* Vorab-Ladeabstand zum Fensterrand */

  var t = "";                  /* aktuelle Route */
  var gen = 0;                 /* veraendert sich bei Tabellenwechsel */
  var rows = [], offs = [], filtered = [], cols = [];
  var tot = 0, base = 0;
  var loading = false;
  var rowH = 30, headH = 0;
  var widths = {};             /* Spaltenname -> Pixelbreite (per Drag) */
  var dragging = null;         /* aktiver Spalten-Resize */
  var dbBase = "db";           /* Basisiname der aktiven Datenbank */

  function colStyle(name) {
    var w = widths[name];
    return w ? " style='width:" + w + "px;min-width:" + w + "px;max-width:" + w + "px'" : "";
  }
  function resizeColumn(ci, w) {
    var cell = thead.rows[0].cells[ci];
    cell.style.width = cell.style.minWidth = cell.style.maxWidth = w + "px";
    for (var r = 0; r < tbody.rows.length; r++) {
      var c = tbody.rows[r].cells[ci];
      c.style.width = c.style.minWidth = c.style.maxWidth = w + "px";
    }
  }

  function esc(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;")
                    .replace(/>/g, "&gt;");
  }
  function escAttr(s) {
    return esc(s).replace(/"/g, "&quot;").replace(/'/g, "&#39;");
  }
  function td(v, num, style) {
    if (v === null || v === undefined)
      return '<td class="null' + (num ? " num" : "") + '"' + style + ">-</td>";
    if (typeof v === "number")
      return '<td class="num"' + style + ">" + v + "</td>";
    return "<td" + style + ">" + esc(v) + "</td>";
  }
  function applyFilter() {
    var q = filt.value.toLowerCase();
    filtered = [];
    rows.forEach(function (r, i) {
      var hit = !q;
      for (var k in r) {
        if (r[k] !== null && r[k] !== undefined &&
            String(r[k]).toLowerCase().indexOf(q) > -1) { hit = true; break; }
      }
      if (hit) filtered.push(i);
    });
  }
  function measure() {
    headH = thead.offsetHeight || 0;
    if (tbody.rows.length) rowH = tbody.offsetHeight / tbody.rows.length;
    if (rowH < 8) rowH = 30;
  }
  function render() {
    applyFilter();
    if (!cols.length) return;
    var head = "<tr>";
    for (var i = 0; i < cols.length; i++) {
      var name = cols[i];
      head += '<th data-key="' + name + '"' + colStyle(name) + ">"
            + esc(name) + '<span class="resizer"></span></th>';
    }
    head += '<th class="fill"></th></tr>';
    thead.innerHTML = head;
    var numeric = {};
    rows.forEach(function (r) {
      for (var c = 0; c < cols.length; c++) {
        if (typeof r[cols[c]] === "number") numeric[cols[c]] = true;
      }
    });
    var out = "";
    filtered.forEach(function (idx) {
      var r = rows[idx];
      out += "<tr>";
      for (var c = 0; c < cols.length; c++)
        out += td(r[cols[c]], numeric[cols[c]], colStyle(cols[c]));
      out += '<td class="fill"></td></tr>';
    });
    tbody.innerHTML = out;
    measure();
    statusUpdate();
  }
  function statusUpdate() {
    if (!cols.length) { status.textContent = "keine Tabelle geladen"; return; }
    var first = base + 1, last = base + rows.length;
    var s = "Zeile " + first + " \u2013 " + last + " von " + tot;
    if (filtered.length !== rows.length)
      s += " \u00b7 " + filtered.length + " Treffer im Fenster";
    if (rows.length < tot)
      s += " (" + rows.length + " geladen)";
    status.textContent = s;
  }
  function visibleOn() {
    var st = box.scrollTop;
    if (!filtered.length)
      return { firstOff: base, lastOff: base };
    var a = Math.max(0, Math.floor(st / rowH) - 1);
    var b = Math.min(filtered.length - 1,
                     Math.floor((st + box.clientHeight) / rowH) + 1);
    return {
      firstOff: offs[filtered[Math.min(a, filtered.length - 1)]],
      lastOff: offs[filtered[Math.min(b, filtered.length - 1)]]
    };
  }
  function trimWindow(firstOff, lastOff) {
    var firstVis = firstOff - base, lastVis = lastOff - base;
    while (rows.length > MAX) {
      var above = Math.max(0, firstVis);
      var below = Math.max(0, rows.length - 1 - lastVis);
      var excess = rows.length - MAX;
      if (above >= below) {
        var d = Math.max(1, Math.min(above, excess));
        base += d;
        rows = rows.slice(d);
        offs = offs.slice(d);
        box.scrollTop -= d * rowH;
        firstVis -= d;
      } else {
        var e = Math.max(1, Math.min(below, excess));
        rows = rows.slice(0, rows.length - e);
        offs = offs.slice(0, offs.length - e);
      }
    }
  }
  function ensureWindow() {
    if (loading) return;
    var v = visibleOn();
    var lastIn = base + rows.length - 1;
    if (lastIn < tot - 1 && v.lastOff > lastIn - MARGIN) {
      loadMore();
      return;
    }
    var firstIn = base;
    if (firstIn > 0 && v.firstOff < firstIn + MARGIN) {
      loadBack();
    }
  }
  function loadMore() {
    if (loading) return;
    var next = base + rows.length;
    if (next >= tot) return;
    var g = gen;
    loading = true;
    status.textContent = "Lade weitere Zeilen ...";
    fetch(t + "?offset=" + next + "&count=" + BATCH)
      .then(function (r) { return r.json(); })
      .then(function (d) {
        if (g !== gen) return;
        if (d && d.rows && d.rows.length) {
          var v = visibleOn();
          var off = next;
          d.rows.forEach(function (row) {
            rows.push(row);
            offs.push(off);
            off += 1;
          });
          if (!cols.length && rows.length) cols = Object.keys(rows[0]);
          if (d.total) tot = d.total;
          trimWindow(v.firstOff, v.lastOff);
          render();
          ensureWindow();
        }
      })
      .catch(function (e) {
        if (g === gen) status.textContent = "Fehler beim Nachladen: " + e;
      })
      .then(function () { loading = false; });
  }
  function loadBack() {
    if (loading) return;
    var prevBase = Math.max(0, base - BATCH);
    if (prevBase === base) return;
    var keep = base - prevBase;
    var g = gen;
    loading = true;
    fetch(t + "?offset=" + prevBase + "&count=" + BATCH)
      .then(function (r) { return r.json(); })
      .then(function (d) {
        if (g !== gen) return;
        if (d && d.rows) {
          var v = visibleOn();
          var ins = d.rows.slice(0, keep);
          base = prevBase;
          var insRows = [], insOffs = [];
          ins.forEach(function (row, i) {
            insRows.push(row);
            insOffs.push(prevBase + i);
          });
          rows = insRows.concat(rows);   /* in korrekter Reihenfolge vorn anhaengen */
          offs = insOffs.concat(offs);
          box.scrollTop += ins.length * rowH;
          if (d.total) tot = d.total;
          trimWindow(v.firstOff, v.lastOff);
          render();
          ensureWindow();
        }
      })
      .catch(function (e) {
        if (g === gen) status.textContent = "Fehler beim Nachladen: " + e;
      })
      .then(function () { loading = false; });
  }
  function pick() {
    t = sel.value;
    if (!t) return;
    gen++;
    loading = false;
    rows = []; offs = []; filtered = []; cols = [];
    tot = 0; base = 0;
    status.textContent = "Lade ...";
    var g = gen;
    fetch(t + "?offset=0&count=" + BATCH)
      .then(function (r) { return r.json(); })
      .then(function (d) {
        if (g !== gen) return;
        rows = d.rows || [];
        offs = []; rows.forEach(function (_, i) { offs.push(i); });
        cols = rows.length ? Object.keys(rows[0]) : [];
        tot = d.total || rows.length;
        render();
        ensureWindow();
      })
      .catch(function (e) {
        if (g === gen) {
          status.textContent = "Fehler beim Laden: " + e;
          status.className = "status err";
        }
      });
  }
  function showRow(rowObj) {
    var fields = "";
    cols.forEach(function (c) {
      var v = rowObj[c];
      var vs = (v === null || v === undefined) ? "" : String(v);
      fields += '<label>' + esc(c) + "</label>" +
                '<input value="' + escAttr(vs) + '" readonly spellcheck="false">';
    });
    dlgForm.innerHTML = fields;
    resetDlgGeometry();
    overlay.hidden = false;
  }
  function closeDlg() {
    overlay.hidden = true;
    dlgForm.innerHTML = "";
  }
  function load() {
    fetch("api/tables").then(function (r) { return r.json(); }).then(function (d) {
      var list = d.tables || [];
      var html = "";
      list.forEach(function (tbl) {
        html += '<option value="' + tbl + '">' + esc(tbl.replace("/api/", "")) + "</option>";
      });
      sel.innerHTML = html;
      if (list.length) pick();
    }).catch(function (e) {
      status.textContent = "Fehler beim Laden der Tabellen: " + e;
      status.className = "status err";
    });
  }
  sel.addEventListener("change", pick);
  filt.addEventListener("input", function () { render(); });
  box.addEventListener("scroll", ensureWindow);
  thead.addEventListener("mousedown", function (e) {
    if (!e.target.classList || !e.target.classList.contains("resizer")) return;
    var th = e.target.closest("th");
    var ci = Array.prototype.indexOf.call(th.parentNode.children, th);
    if (ci < 0 || ci >= cols.length) return;   /* Füllspalte nicht resizen */
    e.preventDefault();
    var startX = e.clientX;
    var startW = widths[cols[ci]] || th.getBoundingClientRect().width;
    dragging = { ci: ci, startX: startX, startW: startW };
    document.body.classList.add("resizing");
    function mv(ev) {
      if (!dragging) return;
      var w = Math.round(dragging.startW + (ev.clientX - dragging.startX));
      if (w < 24) w = 24;
      if (w > 2000) w = 2000;
      resizeColumn(dragging.ci, w);
    }
    function up() {
      document.removeEventListener("mousemove", mv);
      document.removeEventListener("mouseup", up);
      document.body.classList.remove("resizing");
      if (dragging) {
        widths[cols[dragging.ci]] =
          Math.round(thead.rows[0].cells[dragging.ci].getBoundingClientRect().width);
        dragging = null;
      }
    }
    document.addEventListener("mousemove", mv);
    document.addEventListener("mouseup", up);
  });
  tbody.addEventListener("dblclick", function (e) {
    var tr = e.target.closest("tr");
    if (!tr) return;
    var idx = Array.prototype.indexOf.call(tbody.rows, tr);
    if (idx >= 0 && filtered[idx] !== undefined) showRow(rows[filtered[idx]]);
  });
  document.getElementById("dlgClose").addEventListener("click", closeDlg);
  overlay.addEventListener("click", function (e) {
    if (e.target === overlay) closeDlg();
  });
  document.addEventListener("keydown", function (e) {
    if (e.key !== "Escape") return;
    if (!er.hidden) er.hidden = true;
    else closeDlg();
  });
  window.addEventListener("resize", function () { measure(); render(); });
  load();
  fetch("api/health").then(function (r) { return r.json(); }).then(function (h) {
    if (h && h.database) {
      dbBase = h.database;
      sub.textContent = "served by Gorgona \u00b7 SQLite-Datenbank " + h.database;
    }
  }).catch(function () {});

  /* ---- ER-Diagramm ---- */
  var SVGNS = "http://www.w3.org/2000/svg";
  var erData = null, erPos = {}, erKey = "", erEnts = {}, erDrag = null;

  function erRedraw() {
    var canvas = document.getElementById("erCanvas");
    var maxX = 20, maxY = 20;
    Object.keys(erEnts).forEach(function (name) {
      var d = erEnts[name];
      maxX = Math.max(maxX, d.x + d.w);
      maxY = Math.max(maxY, d.y + d.h);
    });
    var cw = maxX + 100, ch = maxY + 100;
    canvas.style.width = cw + "px";
    canvas.style.height = ch + "px";
    var svg = document.getElementById("erEdges");
    if (!svg) {
      svg = document.createElementNS(SVGNS, "svg");
      svg.setAttribute("id", "erEdges");
      canvas.insertBefore(svg, canvas.firstChild);
    }
    svg.setAttribute("width", cw);
    svg.setAttribute("height", ch);
    var html = "";
    html += '<defs><marker id="erArrow" markerWidth="8" markerHeight="8" ' +
            'viewBox="0 0 8 8" refX="8" refY="4" orient="auto">' +
            '<path d="M0,0 L8,4 L0,8 z" fill="#7d8aa0"></path></marker></defs>';
    (erData || []).forEach(function (tc) {
      (tc.fks || []).forEach(function (f) {
        var a = erEnts[tc.name], b = erEnts[f.ref];
        if (!a || !b) return;
        if (f.ref === tc.name) {
          /* Selbstbezug (z. B. EmployeeId -> Employee): kleine Schleife an
             der rechten Kante, Ausbuchtung ~ doppelter Zeilenhoehe */
          var ex = a.x + a.w;
          var y0 = a.y + a.cols[f.from];
          var rr = 18;
          var y2 = Math.min(y0 + rr, a.y + a.h);
          html += '<path d="M' + ex + ',' + y0 + ' C' + (ex + 2 * rr) + ',' + y0 +
                  ' ' + (ex + 2 * rr) + ',' + y2 + ' ' + ex + ',' + y2 + '" ' +
                  'stroke="#8ba4bb" stroke-width="1.5" fill="none" ' +
                  'marker-end="url(#erArrow)"></path>';
          html += '<text x="' + (ex + rr) + '" y="' + (y0 + rr / 2) +
                  '" font-size="9" fill="#5a6a7a">' +
                  esc(f.from + " \u2192 " + f.to) + "</text>";
          return;
        }
        var ay = a.y + a.cols[f.from];
        var by = b.y + b.cols[f.to];
        var acx = a.x + a.w / 2, bcx = b.x + b.w / 2;
        var x1, x2;
        if (bcx >= acx) {
          x1 = a.x + a.w;      /* Ziel rechts: Ausgang an rechter Kante */
          x2 = b.x;            /* Ende an der linken (zugewandten) Boxkante */
        } else {
          x1 = a.x;            /* Ziel links: Ausgang an linker Kante */
          x2 = b.x + b.w;      /* Ende an der rechten (zugewandten) Boxkante */
        }
        html += '<line x1="' + x1 + '" y1="' + ay + '" x2="' + x2 +
                '" y2="' + by + '" stroke="#8ba4bb" stroke-width="1.5" ' +
                'marker-end="url(#erArrow)"></line>';
        html += '<text x="' + ((x1 + x2) / 2 + 8) + '" y="' +
                ((ay + by) / 2 - 4) + '" font-size="9" fill="#5a6a7a">' +
                esc(f.from + " \u2192 " + f.to) + "</text>";
      });
    });
    svg.innerHTML = html;
  }

  /* Anfangspositionen: geschichtetes Layout (referenzierte Tabellen rechts),
     Ordnung je Ebene ueber den Median der Nachbarn in der Nachbarebene
     (Sugiyama-Heringschwaerzer) -> moeglichst wenige Kreuze zwischen den
     Fremdschluessel-Kanten. */
  function erInitialPositions() {
    var tables = erData || [];
    var idx = {};
    tables.forEach(function (t, i) { idx[t.name] = i; });
    var fwd = {}, rev = {};
    tables.forEach(function (t) {
      fwd[t.name] = [];
      rev[t.name] = rev[t.name] || [];
      (t.fks || []).forEach(function (f) {
        fwd[t.name].push(f.ref);
        (rev[f.ref] = rev[f.ref] || []).push(t.name);
      });
    });
    var layer = {}, state = {};
    function lay(t) {
      if (state[t] === 1) return layer[t];
      if (state[t] === 2) return 0;
      state[t] = 2;
      var best = 0;
      (fwd[t] || []).forEach(function (r) {
        if (idx[r] !== undefined) best = Math.max(best, lay(r) + 1);
      });
      layer[t] = best;
      state[t] = 1;
      return best;
    }
    tables.forEach(function (t) {
      if (((rev[t.name] || []).length === 0) && (fwd[t.name] || []).length > 0)
        lay(t.name);
    });
    var groups = {};
    var gmax = 0;
    tables.forEach(function (t) {
      if (layer[t.name] === undefined) return;
      var g = layer[t.name];
      (groups[g] = groups[g] || []).push(t.name);
      if (g > gmax) gmax = g;
    });
    function median(list) {
      if (!list.length) return null;
      list = list.slice().sort(function (a, b) { return a - b; });
      return list[Math.floor(list.length / 2)];
    }
    function sortByMed(list, getMed) {
      var med = {};
      list.forEach(function (t) { med[t] = getMed(t); });
      list.sort(function (a, b) {
        var ma = median(med[a] || []), mb = median(med[b] || []);
        if (ma === null && mb === null) return 0;
        if (ma === null) return 1;
        if (mb === null) return -1;
        return ma - mb;
      });
    }
    var k;
    for (k = 0; k < gmax; k++) {
      var cur = groups[k] || [], nxt = groups[k + 1] || [];
      if (!nxt.length) continue;
      var pos = {};
      cur.forEach(function (t, i) { pos[t] = i; });
      sortByMed(nxt, function (t) {
        return (rev[t] || []).map(function (s) { return pos[s]; })
                             .filter(function (v) { return v !== undefined; });
      });
    }
    for (k = gmax; k >= 1; k--) {
      var bCur = groups[k] || [], bNxt = groups[k + 1] || [];
      var bPos = {};
      bNxt.forEach(function (t, i) { bPos[t] = i; });
      sortByMed(bCur, function (t) {
        return (fwd[t] || []).map(function (r) { return bPos[r]; })
                             .filter(function (v) { return v !== undefined; });
      });
    }
    var iso = [];
    tables.forEach(function (t) {
      if (layer[t.name] === undefined) iso.push(t.name);
    });
    var DX = 270, DY = 150, X0 = 60, Y0 = 60;
    var out = {};
    for (var g = 0; g <= gmax; g++) {
      (groups[g] || []).forEach(function (t, i) {
        out[t] = { x: X0 + g * DX, y: Y0 + i * DY };
      });
    }
    iso.forEach(function (t, i) {
      out[t] = { x: X0 + (gmax + 1) * DX, y: Y0 + i * DY };
    });
    return out;
  }

  function buildEr() {
    erEnts = {};
    erKey = "gorgona.er." + dbBase;
    try {
      erPos = JSON.parse(localStorage.getItem(erKey)) || {};
    } catch (e) { erPos = {}; }
    var canvas = document.getElementById("erCanvas");
    canvas.innerHTML = "";
    var initPos = erInitialPositions();
    (erData || []).forEach(function (tc, i) {
      var p = erPos[tc.name] || initPos[tc.name] || { x: 0, y: 0 };
      var ent = document.createElement("div");
      ent.className = "ent";
      ent.style.left = p.x + "px";
      ent.style.top = p.y + "px";
      var fkCols = {};
      (tc.fks || []).forEach(function (f) { fkCols[f.from] = f.ref; });
      var inner = '<div class="t">' + esc(tc.name) + "</div>";
      tc.cols.forEach(function (c, ci) {
        inner += '<div class="c' + (c.pk ? " pk" : "") +
                 (fkCols[c.name] ? " fk" : "") + '" data-name="' + escAttr(c.name) + '">' +
                 esc(c.name) + (fkCols[c.name]
                   ? " \u2192 " + esc(fkCols[c.name]) : "") + "</div>";
      });
      ent.innerHTML = inner;
      ent.addEventListener("mousedown", function (e) { erStartDrag(e, ent, tc.name); });
      canvas.appendChild(ent);
      (function (ent, tc) {
        var head = ent.querySelector(".t");
        var headH = head ? head.offsetHeight : 26;
        var firstC = ent.querySelector(".c");
        var rowH = firstC ? firstC.offsetHeight : 19;
        var cols = {};
        ent.querySelectorAll(".c").forEach(function (el, j) {
          cols[el.getAttribute("data-name")] = headH + j * rowH + rowH / 2;
        });
        erEnts[tc.name] = { x: ent.offsetLeft, y: ent.offsetTop,
                            w: ent.offsetWidth, h: ent.offsetHeight, cols: cols };
      })(ent, tc);
    });
    erRedraw();
  }

  function erStartDrag(e, ent, name) {
    if (e.button !== 0) return;
    e.preventDefault();
    var sx = e.clientX, sy = e.clientY;
    var ox = parseInt(ent.style.left, 10), oy = parseInt(ent.style.top, 10);
    erDrag = { ent: ent, name: name, ox: ox, oy: oy, sx: sx, sy: sy };
    ent.classList.add("dragging");
    document.body.classList.add("resizing");
    function mv(ev) {
      if (!erDrag) return;
      var x = erDrag.ox + (ev.clientX - erDrag.sx);
      var y = erDrag.oy + (ev.clientY - erDrag.sy);
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      ent.style.left = x + "px";
      ent.style.top = y + "px";
      var d = erEnts[name];
      if (d) {
        d.x = x;
        d.y = y;
        erRedraw();
      }
    }
    function up() {
      document.removeEventListener("mousemove", mv);
      document.removeEventListener("mouseup", up);
      ent.classList.remove("dragging");
      document.body.classList.remove("resizing");
      if (erDrag) {
        erPos[name] = { x: parseInt(ent.style.left, 10), y: parseInt(ent.style.top, 10) };
        try { localStorage.setItem(erKey, JSON.stringify(erPos)); } catch (e2) {}
        erDrag = null;
      }
    }
    document.addEventListener("mousemove", mv);
    document.addEventListener("mouseup", up);
  }

  function erOpen() {
    resetDlgGeometry();
    er.hidden = false;
    if (erData) { buildEr(); return; }
    status.textContent = "Lade Schema ...";
    fetch("api/schema").then(function (r) { return r.json(); }).then(function (d) {
      erData = (d && d.tables) ? d.tables : [];
      buildEr();
    }).catch(function (err) {
      status.textContent = "Fehler beim Laden des Schemas: " + err;
      er.hidden = true;
    });
  }

  /* Dialoge (Datensatz und ER-Diagramm) frei im Fenster verschieben: beim
     Greifen wird der Dialog von der Flex-Zentrierung auf position:fixed
     umgestellt, damit er exakt dort bleibt, wo die Maus steht. */
  function dragDialog(dlg, handle) {
    if (!dlg || !handle || !handle.addEventListener) return;
    handle.addEventListener("mousedown", function (e) {
      if (e.button !== 0) return;
      if (e.target.closest && e.target.closest("button")) return;
      var r = dlg.getBoundingClientRect();
      dlg.style.position = "fixed";
      dlg.style.left = r.left + "px";
      dlg.style.top = r.top + "px";
      dlg.style.margin = "0";
      document.body.classList.add("resizing");
      var sx = e.clientX, sy = e.clientY, ox = r.left, oy = r.top;
      function mv(ev) {
        dlg.style.left = Math.max(0, ox + (ev.clientX - sx)) + "px";
        dlg.style.top = Math.max(0, oy + (ev.clientY - sy)) + "px";
      }
      function up() {
        document.removeEventListener("mousemove", mv);
        document.removeEventListener("mouseup", up);
        document.body.classList.remove("resizing");
      }
      document.addEventListener("mousemove", mv);
      document.addEventListener("mouseup", up);
      e.preventDefault();
    });
  }

  function resizeDialog(dlg, grip) {
    if (!dlg || !grip || !grip.addEventListener) return;
    grip.addEventListener("mousedown", function (e) {
      if (e.button !== 0) return;
      e.preventDefault();
      e.stopPropagation();
      var r = dlg.getBoundingClientRect();
      dlg.style.position = "fixed";
      dlg.style.left = r.left + "px";
      dlg.style.top = r.top + "px";
      dlg.style.margin = "0";
      dlg.style.maxWidth = "none";
      dlg.style.maxHeight = "none";
      dlg.style.width = r.width + "px";
      dlg.style.height = r.height + "px";
      document.body.classList.add("resizing");
      var sx = e.clientX, sy = e.clientY;
      function mv(ev) {
        var nw = r.width + (ev.clientX - sx);
        var nh = r.height + (ev.clientY - sy);
        if (nw < 280) nw = 280;
        if (nh < 140) nh = 140;
        dlg.style.width = nw + "px";
        dlg.style.height = nh + "px";
      }
      function up() {
        document.removeEventListener("mousemove", mv);
        document.removeEventListener("mouseup", up);
        document.body.classList.remove("resizing");
      }
      document.addEventListener("mousemove", mv);
      document.addEventListener("mouseup", up);
    });
  }

  var dlgEl = document.querySelector("#overlay .dlg"),
      erDlgEl = document.querySelector("#er .erDlg");
  function resetDlgGeometry() {
    [dlgEl, erDlgEl].forEach(function (el) {
      if (!el || !el.style) return;
      el.style.position = "";
      el.style.left = "";
      el.style.top = "";
      el.style.margin = "";
      el.style.width = "";
      el.style.height = "";
      el.style.maxWidth = "";
      el.style.maxHeight = "";
    });
  }

  erBtn.addEventListener("click", erOpen);
  erClose.addEventListener("click", function () { er.hidden = true; });
  er.addEventListener("click", function (e) {
    if (e.target === er) er.hidden = true;
  });
  var erReset = document.getElementById("erReset");
  if (erReset) erReset.addEventListener("click", function () {
    try { localStorage.removeItem(erKey); } catch (e2) {}
    erPos = {};
    if (erData) buildEr();
  });

  dragDialog(dlgEl, dlgEl ? dlgEl.querySelector("h2") : null);
  resizeDialog(dlgEl, document.getElementById("dlgGrip"));
  dragDialog(erDlgEl, erDlgEl ? erDlgEl.querySelector(".erHead") : null);
  resizeDialog(erDlgEl, document.getElementById("erGrip"));
}());