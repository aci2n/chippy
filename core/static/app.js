(function () {
  var api = typeof window.CHIPPY_API_URL === "string" ? window.CHIPPY_API_URL : "";

  document.getElementById("api-url").textContent = api || "(this server)";

  function setMsg(id, text, ok) {
    var el = document.getElementById(id);
    el.textContent = text;
    el.className = "msg " + (ok ? "ok" : "err");
  }

  function apiFetch(path, opts) {
    return fetch(api + path, opts).then(function (r) {
      return r.json().then(function (body) {
        if (!r.ok) {
          var err = new Error(body.error || r.statusText);
          err.status = r.status;
          throw err;
        }
        return body;
      });
    });
  }

  apiFetch("/health")
    .then(function () {
      var el = document.getElementById("health");
      el.textContent = "Backend OK";
      el.className = "status ok";
    })
    .catch(function (e) {
      var el = document.getElementById("health");
      el.textContent = "Backend unreachable: " + e.message;
      el.className = "status err";
    });

  document.getElementById("btn-validate").onclick = function () {
    setMsg("validate-msg", "…", true);
    apiFetch("/api/v1/chain/validate")
      .then(function () {
        setMsg("validate-msg", "Chain valid", true);
      })
      .catch(function (e) {
        setMsg("validate-msg", e.message, false);
      });
  };

  document.getElementById("btn-balance").onclick = function () {
    var addr = document.getElementById("balance-addr").value.trim();
    if (addr.length !== 64) {
      setMsg("balance-msg", "Address must be 64 hex characters", false);
      return;
    }
    setMsg("balance-msg", "…", true);
    apiFetch("/api/v1/balance/" + addr)
      .then(function (data) {
        setMsg("balance-msg", "Balance: " + data.balance, true);
      })
      .catch(function (e) {
        setMsg("balance-msg", e.message, false);
      });
  };

  document.getElementById("btn-mint").onclick = function () {
    var body = {
      to: document.getElementById("mint-to").value.trim(),
      amount: Number(document.getElementById("mint-amount").value),
      sig: document.getElementById("mint-sig").value.trim()
    };
    setMsg("mint-msg", "…", true);
    apiFetch("/api/v1/mint", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    })
      .then(function () {
        setMsg("mint-msg", "Mint submitted", true);
      })
      .catch(function (e) {
        setMsg("mint-msg", e.message, false);
      });
  };

  document.getElementById("btn-transfer").onclick = function () {
    var body = {
      from: document.getElementById("xfer-from").value.trim(),
      to: document.getElementById("xfer-to").value.trim(),
      amount: Number(document.getElementById("xfer-amount").value),
      sig: document.getElementById("xfer-sig").value.trim()
    };
    setMsg("xfer-msg", "…", true);
    apiFetch("/api/v1/transfer", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    })
      .then(function () {
        setMsg("xfer-msg", "Transfer submitted", true);
      })
      .catch(function (e) {
        setMsg("xfer-msg", e.message, false);
      });
  };
})();
