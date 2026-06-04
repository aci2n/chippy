(function () {
  var api = typeof window.CHIPPY_API_URL === "string" ? window.CHIPPY_API_URL : "";
  var wallet = null;

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

  function walletSecret() {
    return ChippyCrypto.normalizeSecretHex(document.getElementById("wallet-secret").value);
  }

  function requireWalletSecret() {
    var sec = walletSecret();
    if (sec.length !== ChippyCrypto.SEC_LEN) {
      throw new Error(
        "Set wallet secret (" +
          ChippyCrypto.SEC_LEN +
          " hex chars): use Keygen, or paste only the secret= line from chippy keygen"
      );
    }
    return sec;
  }

  function cryptoErrorMessage(e) {
    if (e && e.name === "DOMException") {
      return (
        (e.message || "Web Crypto error") +
        " — try Generate keypair or paste a full 128-char libsodium secret"
      );
    }
    return e && e.message ? e.message : String(e);
  }

  function syncWalletFields(w) {
    document.getElementById("wallet-addr").value = w.address;
    document.getElementById("wallet-secret").value = w.secret;
    wallet = w;
  }

  if (!ChippyCrypto.ed25519Available()) {
    setMsg("keygen-msg", "Ed25519 Web Crypto not available in this browser", false);
  }

  document.getElementById("btn-keygen").onclick = function () {
    setMsg("keygen-msg", "…", true);
    ChippyCrypto.generateKeypair()
      .then(function (w) {
        syncWalletFields(w);
        setMsg("keygen-msg", "Keypair generated (save secret off-server)", true);
      })
      .catch(function (e) {
        setMsg("keygen-msg", cryptoErrorMessage(e), false);
      });
  };

  document.getElementById("btn-load-wallet").onclick = function () {
    setMsg("keygen-msg", "…", true);
    var sec = walletSecret();
    ChippyCrypto.importWallet(sec)
      .then(function (w) {
        syncWalletFields(w);
        setMsg("keygen-msg", "Wallet loaded", true);
      })
      .catch(function (e) {
        setMsg("keygen-msg", cryptoErrorMessage(e), false);
      });
  };

  document.getElementById("btn-fill-from").onclick = function () {
    var addr = document.getElementById("wallet-addr").value.trim();
    if (addr.length !== ChippyCrypto.ADDR_LEN) {
      setMsg("xfer-msg", "Generate or load a wallet first", false);
      return;
    }
    document.getElementById("xfer-from").value = addr;
    setMsg("xfer-msg", "From address filled", true);
  };

  document.getElementById("btn-sign-mint").onclick = function () {
    var to = document.getElementById("mint-to").value.trim();
    var amount = document.getElementById("mint-amount").value;
    setMsg("mint-msg", "…", true);
    Promise.resolve()
      .then(function () {
        return ChippyCrypto.signMint(requireWalletSecret(), to, amount, wallet);
      })
      .then(function (sig) {
        document.getElementById("mint-sig").value = sig;
        setMsg("mint-msg", "Mint signed", true);
      })
      .catch(function (e) {
        setMsg("mint-msg", cryptoErrorMessage(e), false);
      });
  };

  document.getElementById("btn-sign-transfer").onclick = function () {
    var from = document.getElementById("xfer-from").value.trim();
    var to = document.getElementById("xfer-to").value.trim();
    var amount = document.getElementById("xfer-amount").value;
    setMsg("xfer-msg", "…", true);
    Promise.resolve()
      .then(function () {
        return ChippyCrypto.signTransfer(requireWalletSecret(), from, to, amount, wallet);
      })
      .then(function (sig) {
        document.getElementById("xfer-sig").value = sig;
        setMsg("xfer-msg", "Transfer signed", true);
      })
      .catch(function (e) {
        setMsg("xfer-msg", cryptoErrorMessage(e), false);
      });
  };

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
    if (addr.length !== ChippyCrypto.ADDR_LEN) {
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
    if (body.sig.length !== ChippyCrypto.SIG_LEN) {
      setMsg("mint-msg", "Signature must be " + ChippyCrypto.SIG_LEN + " hex characters", false);
      return;
    }
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
    if (body.sig.length !== ChippyCrypto.SIG_LEN) {
      setMsg("xfer-msg", "Signature must be " + ChippyCrypto.SIG_LEN + " hex characters", false);
      return;
    }
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
