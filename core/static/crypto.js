/* Chippy client crypto — Ed25519 via Web Crypto, compatible with libsodium CLI */
var ChippyCrypto = (function () {
  "use strict";

  var ADDR_LEN = 64;
  var SEC_LEN = 128;
  var SIG_LEN = 128;

  function hexEncode(bytes) {
    var out = "";
    for (var i = 0; i < bytes.length; i++) {
      out += ("0" + bytes[i].toString(16)).slice(-2);
    }
    return out;
  }

  function hexDecode(hex) {
    if (!/^[0-9a-fA-F]*$/.test(hex) || hex.length % 2 !== 0) {
      throw new Error("invalid hex");
    }
    var out = new Uint8Array(hex.length / 2);
    for (var i = 0; i < out.length; i++) {
      out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
    }
    return out;
  }

  function normalizeSecretHex(secretHex) {
    if (secretHex == null) {
      return "";
    }
    return String(secretHex).replace(/\s+/g, "").toLowerCase();
  }

  function b64UrlEncode(bytes) {
    var bin = "";
    for (var i = 0; i < bytes.length; i++) {
      bin += String.fromCharCode(bytes[i]);
    }
    return btoa(bin).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
  }

  function b64UrlDecode(s) {
    var pad = (4 - (s.length % 4)) % 4;
    var b64 = s.replace(/-/g, "+").replace(/_/g, "/") + "=".repeat(pad);
    var raw = atob(b64);
    var out = new Uint8Array(raw.length);
    for (var i = 0; i < raw.length; i++) {
      out[i] = raw.charCodeAt(i);
    }
    return out;
  }

  function parseSecret(secretHex) {
    var norm = normalizeSecretHex(secretHex);
    if (norm.length !== SEC_LEN) {
      throw new Error("secret must be " + SEC_LEN + " hex characters");
    }
    var sk = hexDecode(norm);
    if (sk.length !== 64) {
      throw new Error("invalid secret");
    }
    return { seed: sk.slice(0, 32), pub: sk.slice(32, 64), hex: norm };
  }

  function assertAddress(addr, label) {
    if (addr.length !== ADDR_LEN || !/^[0-9a-fA-F]{64}$/.test(addr)) {
      throw new Error((label || "address") + " must be 64 hex characters");
    }
  }

  function formatAmount(amount) {
    var n = Number(amount);
    if (!Number.isFinite(n) || n < 0 || !Number.isInteger(n)) {
      throw new Error("amount must be a non-negative integer");
    }
    return String(n);
  }

  function mintPayload(to, amount) {
    return "mint\n" + to + "\n" + formatAmount(amount);
  }

  function transferPayload(from, to, amount) {
    return "transfer\n" + from + "\n" + to + "\n" + formatAmount(amount);
  }

  function libsodiumSecretHex(seed, pub) {
    var sk = new Uint8Array(64);
    sk.set(seed, 0);
    sk.set(pub, 32);
    return hexEncode(sk);
  }

  /*
   * Import libsodium 64-byte secret (seed||pub) as Ed25519 signing key.
   * JWK import works in Firefox and Chrome; raw seed import is not portable.
   */
  function importPrivateKeyFromParts(parts) {
    var jwk = {
      kty: "OKP",
      crv: "Ed25519",
      d: b64UrlEncode(parts.seed),
      x: b64UrlEncode(parts.pub)
    };
    return crypto.subtle.importKey("jwk", jwk, { name: "Ed25519" }, false, ["sign"]);
  }

  function signPayload(privateKey, payloadStr) {
    var data = new TextEncoder().encode(payloadStr);
    return crypto.subtle.sign("Ed25519", privateKey, data).then(function (sig) {
      if (sig.byteLength !== 64) {
        throw new Error("unexpected signature length");
      }
      return hexEncode(new Uint8Array(sig));
    });
  }

  function exportSeedAndPub(pair) {
    return crypto.subtle
      .exportKey("raw", pair.publicKey)
      .then(function (pubRaw) {
        var pub = new Uint8Array(pubRaw);
        return crypto.subtle.exportKey("raw", pair.privateKey).then(
          function (seedRaw) {
            return { seed: new Uint8Array(seedRaw), pub: pub };
          },
          function () {
            return crypto.subtle.exportKey("jwk", pair.privateKey).then(function (jwk) {
              if (!jwk.d || !jwk.x) {
                throw new Error("cannot export Ed25519 key material");
              }
              return { seed: b64UrlDecode(jwk.d), pub: b64UrlDecode(jwk.x) };
            });
          }
        );
      });
  }

  function ed25519Available() {
    return typeof crypto !== "undefined" && crypto.subtle && crypto.subtle.generateKey;
  }

  function resolvePrivateKey(secretHex, cached) {
    if (cached && cached.privateKey && normalizeSecretHex(cached.secret) === normalizeSecretHex(secretHex)) {
      return Promise.resolve(cached.privateKey);
    }
    return importPrivateKeyFromParts(parseSecret(secretHex));
  }

  return {
    ADDR_LEN: ADDR_LEN,
    SEC_LEN: SEC_LEN,
    SIG_LEN: SIG_LEN,
    normalizeSecretHex: normalizeSecretHex,
    ed25519Available: ed25519Available,

    generateKeypair: function () {
      if (!ed25519Available()) {
        return Promise.reject(new Error("Ed25519 not supported in this browser"));
      }
      return crypto.subtle.generateKey("Ed25519", true, ["sign"]).then(function (pair) {
        return exportSeedAndPub(pair).then(function (material) {
          if (material.seed.length !== 32 || material.pub.length !== 32) {
            throw new Error("unexpected Ed25519 key length");
          }
          return {
            address: hexEncode(material.pub),
            secret: libsodiumSecretHex(material.seed, material.pub),
            privateKey: pair.privateKey
          };
        });
      });
    },

    addressFromSecret: function (secretHex) {
      return hexEncode(parseSecret(secretHex).pub);
    },

    importWallet: function (secretHex) {
      var parts = parseSecret(secretHex);
      return importPrivateKeyFromParts(parts).then(function (privateKey) {
        return {
          address: hexEncode(parts.pub),
          secret: parts.hex,
          privateKey: privateKey
        };
      });
    },

    signMint: function (secretHex, to, amount, cachedWallet) {
      assertAddress(to, "to");
      return resolvePrivateKey(secretHex, cachedWallet).then(function (privateKey) {
        return signPayload(privateKey, mintPayload(to, amount));
      });
    },

    signTransfer: function (secretHex, from, to, amount, cachedWallet) {
      assertAddress(from, "from");
      assertAddress(to, "to");
      var parts = parseSecret(secretHex);
      if (hexEncode(parts.pub) !== from) {
        return Promise.reject(new Error("secret does not match from address"));
      }
      return resolvePrivateKey(secretHex, cachedWallet).then(function (privateKey) {
        return signPayload(privateKey, transferPayload(from, to, amount));
      });
    }
  };
})();
