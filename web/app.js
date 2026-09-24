/* Gorgona DB Browser fuer Gorgona (Unterprojekt in web/).
   Da die Seite nun als echte Datei von Disk geliefert wird, gelten hier
   keine ga68-String-Beschraenkungen mehr: normales JavaScript. */
(function () {
  "use strict";
  var sel = document.getElementById("sel"),
      filt = document.getElementById("f"),
      thead = document.getElementById("th"),
      tbody = document.getElementById("tb"),
      status = document.getElementById("status"),
      sub = document.getElementById("sub");
  var rows = [], filtered = [], cols = [];
  var page = 0, per = 25, sortKey = "", sortDir = 1;

  function esc(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;")
                    .replace(/>/g, "&gt;");
  }
  function td(v, num) {
    if (v === null || v === undefined)
      return '<td class="null' + (num ? " num" : "") + '">-</td>';
    if (typeof v === "number") return '<td class="num">' + v + "</td>";
    return "<td>" + esc(v) + "</td>";
  }
  function applyFilter() {
    var q = filt.value.toLowerCase();
    filtered = [];
    rows.forEach(function (r) {
      var hit = !q;
      for (var k in r) {
        if (r[k] !== null && r[k] !== undefined &&
            String(r[k]).toLowerCase().indexOf(q) > -1) { hit = true; break; }
      }
      if (hit) filtered.push(r);
    });
  }
  function sortRows() {
    if (!sortKey) return;
    var k = sortKey, d = sortDir;
    filtered.sort(function (a, b) {
      var x = a[k], y = b[k];
      if (x === null && y === null) return 0;
      if (x === null) return 1;
      if (y === null) return -1;
      if (typeof x === "number" && typeof y === "number") return (x - y) * d;
      x = String(x).toLowerCase();
      y = String(y).toLowerCase();
      return (x < y ? -1 : x > y ? 1 : 0) * d;
    });
  }
  function render() {
    applyFilter();
    sortRows();
    var pages = Math.max(1, Math.ceil(filtered.length / per));
    if (page > pages - 1) page = pages - 1;
    var slice = filtered.slice(page * per, page * per + per);
    var numeric = {};
    for (r = 0; r < rows.length; r++) {
      for (c = 0; c < cols.length; c++) {
        if (typeof rows[r][cols[c]] === "number") numeric[cols[c]] = true;
      }
    }
    var head = "<tr>";
    var i, c, r;
    for (i = 0; i < cols.length; i++) {
      var name = cols[i];
      head += '<th data-key="' + name + '">' + esc(name) +
              (name === sortKey ? (sortDir === 1 ? " &uarr;" : " &darr;") : "") + "</th>";
    }
    head += '<th class="fill"></th>';
    head += "</tr>";
    thead.innerHTML = head;
    var out = "";
    for (r = 0; r < slice.length; r++) {
      out += "<tr>";
      for (c = 0; c < cols.length; c++) {
        out += td(slice[r][cols[c]], numeric[cols[c]]);
      }
      out += '<td class="fill"></td>';
      out += "</tr>";
    }
    tbody.innerHTML = out;
    var s = filtered.length + " Zeile(n) von " + rows.length + " geladenen Zeilen";
    if (filtered.length > per) s += " - Seite " + (page + 1) + " von " + pages;
    status.textContent = s;
  }
  function pick() {
    var t = sel.value;
    if (!t) return;
    status.textContent = "Lade " + t + " ...";
    fetch(t).then(function (r) { return r.json(); }).then(function (d) {
      rows = d.rows || [];
      cols = rows.length ? Object.keys(rows[0]) : [];
      sortKey = "";
      sortDir = 1;
      page = 0;
      render();
    }).catch(function (e) {
      status.textContent = "Fehler beim Laden: " + e;
      status.className = "status err";
    });
  }
  function load() {
    fetch("api/tables").then(function (r) { return r.json(); }).then(function (d) {
      var list = d.tables || [];
      var html = "";
      list.forEach(function (t) {
        html += '<option value="' + t + '">' + esc(t.replace("/api/", "")) + "</option>";
      });
      sel.innerHTML = html;
      if (list.length) pick();
    }).catch(function (e) {
      status.textContent = "Fehler beim Laden der Tabellen: " + e;
      status.className = "status err";
    });
    /* Aktive Datenbank (Basisname) im Untertitel anzeigen */
    fetch("api/health").then(function (r) { return r.json(); }).then(function (h) {
      if (h && h.database) {
        sub.textContent = "served by Gorgona \u00b7 SQLite-Datenbank " + h.database;
      }
    }).catch(function () {});
  }
  sel.addEventListener("change", pick);
  filt.addEventListener("input", function () { page = 0; render(); });
  thead.addEventListener("click", function (e) {
    var el = e.target.closest("th");
    if (!el) return;
    var k = el.getAttribute("data-key");
    if (!k) return;
    if (k === sortKey) sortDir = -sortDir; else { sortKey = k; sortDir = 1; }
    render();
  });
  document.getElementById("prev").addEventListener("click", function () {
    if (page > 0) { page--; render(); }
  });
  document.getElementById("next").addEventListener("click", function () {
    if (page < Math.ceil(filtered.length / per) - 1) { page++; render(); }
  });
  load();
}());