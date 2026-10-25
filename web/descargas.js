// Los enlaces de descarga, de la ultima version publicada en GitHub. Asi la
// pagina no cambia con cada version: cada enlace dice el patron del nombre
// del fichero (data-patron) y aqui se busca en la release.
(function () {
  "use strict";
  var REPO = "davidesg/atsw-gui";
  var RELEASES = "https://github.com/" + REPO + "/releases";

  function mb(bytes) {
    return Math.round(bytes / 1048576) + " MB";
  }

  // El sistema de quien visita, para destacar su tarjeta.
  function sistema() {
    var p = (navigator.userAgentData && navigator.userAgentData.platform) ||
            navigator.platform || navigator.userAgent || "";
    if (/android/i.test(navigator.userAgent || "")) return null;
    if (/win/i.test(p)) return "windows";
    if (/mac/i.test(p)) return "mac";
    if (/linux/i.test(p)) return "linux";
    return null;
  }

  function sinVersion(texto) {
    document.getElementById("estado").innerHTML = texto;
    document.querySelectorAll("a.pendiente").forEach(function (a) {
      a.href = RELEASES;
    });
  }

  var so = sistema();
  if (so) {
    var t = document.querySelector('.tarjeta[data-so="' + so + '"]');
    if (t) t.classList.add("tuya");
  }

  // La version HTML del manual, sin anidar un enlace dentro de otro.
  document.querySelectorAll(".sub[data-href]").forEach(function (s) {
    s.addEventListener("click", function (e) {
      e.preventDefault();
      location.href = s.getAttribute("data-href");
    });
  });

  fetch("https://api.github.com/repos/" + REPO + "/releases?per_page=10")
    .then(function (r) {
      if (!r.ok) throw new Error(r.status);
      return r.json();
    })
    .then(function (lista) {
      var rel = lista.filter(function (x) { return !x.draft; })[0];
      if (!rel) {
        sinVersion("Todavía no hay ninguna versión publicada.");
        return;
      }
      var fecha = new Date(rel.published_at).toLocaleDateString("es-ES",
        { year: "numeric", month: "long", day: "numeric" });
      document.getElementById("estado").innerHTML =
        "Versión <b>" + rel.tag_name.replace(/^v/, "") + "</b>, del " + fecha +
        (rel.prerelease ? " · de prueba" : "") +
        ' · <a href="' + rel.html_url + '">notas</a>';
      document.getElementById("version-nota").textContent =
        "Versión " + rel.tag_name.replace(/^v/, "") +
        (rel.prerelease ? ", de prueba," : "") + " para Windows, macOS y Linux.";

      document.querySelectorAll("a[data-patron]").forEach(function (a) {
        var re = new RegExp(a.getAttribute("data-patron"));
        var f = rel.assets.filter(function (x) { return re.test(x.name); })[0];
        if (f) {
          a.href = f.browser_download_url;
          a.classList.remove("pendiente");
          var tam = document.createElement("small");
          tam.textContent = " " + mb(f.size);
          a.appendChild(tam);
        } else {
          a.href = rel.html_url;
          a.title = "No está en esta versión";
          a.classList.add("falta");
        }
      });

      // Cada tarjeta, SU video: ATSW-<data-video>.mp4. Antes se tomaba el
      // primero que hubiera, y con dos la tarjeta enlazaba el que no era.
      document.querySelectorAll("a[data-video]").forEach(function (a) {
        var nombre = "ATSW-" + a.getAttribute("data-video") + ".mp4";
        var v = rel.assets.filter(function (x) { return x.name === nombre; })[0];
        if (v) {
          a.href = v.browser_download_url;
          a.textContent = "Descargar el vídeo (MP4, " + mb(v.size) + ")";
        }
      });

      // El boton grande de la portada baja directamente lo de tu sistema,
      // si no hay duda de cual es (en Mac la hay: chip y version).
      if (so === "windows") {
        var w = document.querySelector('a[data-patron^="windows-x64-setup"]');
        if (w && !w.classList.contains("falta")) {
          var b = document.getElementById("boton-principal");
          b.href = w.href;
          b.textContent = "Descargar para Windows";
        }
      }
    })
    .catch(function () {
      sinVersion('No he podido consultar GitHub. Las descargas están en ' +
                 '<a href="' + RELEASES + '">la página de versiones</a>.');
    });
})();
