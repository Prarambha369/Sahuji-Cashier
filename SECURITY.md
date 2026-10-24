# Security Policy

## Admin Credential Handling

### What changed
Earlier versions shipped a hardcoded default admin password (`admin`) and stored
the password in plaintext in `adminpwd.txt`. This removed a known,
always-available default credential and any readable copy of the admin password.

### Current design
- **No default password.** On first run the app forces admin password creation:
  it prompts for a password and a confirmation, and rejects anything shorter
  than 8 characters (or whitespace-only) until a valid pair is entered.
- **Salted hashing.** The password is stored only as
  `SHA-256(salt || password)` next to a random 16-byte salt generated with
  OpenSSL's CSPRNG (`RAND_bytes`). The record lives in `adminpwd.dat` as a
  `salt_hex:hash_hex` line — the plaintext password is never written to disk.
- **Verification.** Logins and password changes recompute the salted hash and
  compare it against the stored hash using a constant-time comparison.
- **Password changes** require a matched confirmation with the same minimum
  length, and always generate a fresh salt.
- **Legacy migration.** An old plaintext `adminpwd.txt` is ignored and deleted
  as soon as a new hashed record is stored; its password (`admin` or otherwise)
  no longer works.

### Notes and limits
- The build now requires OpenSSL and links with `-lcrypto`:
  `g++ -std=c++17 sahuji.cpp -o sahuji -lcrypto`.
- SHA-256 with a simple salt is acceptable for this offline console app, but it
  is not a substitute for a PBKDF2/bcrypt/scrypt/argon2 KDF. If hardening this
  further, hash with `PKCS5_PBKDF2_HMAC` and a high iteration count.
- Data files (`products.dat`, `customers.dat`, `bills.dat`, `adminpwd.dat`)
  are unencrypted; protect the working directory with OS file permissions.
- Please do not report credential files or terminal transcripts in public
  issues.
