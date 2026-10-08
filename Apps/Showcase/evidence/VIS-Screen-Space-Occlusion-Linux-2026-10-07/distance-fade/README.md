# Distant contact precision: Linux native evidence

Contact occlusion fades between 32 and 64 world units to avoid false contacts caused by RGBA16F distance quantization on distant sky. New native fixtures verify distant geometry remains unchanged when enabling occlusion; the existing contact, planar, HDR and exact restoration checks still pass. The combined frozen runtime includes the separately verified landmark-cypress placement.

Full Linux configure/build and 104/104 tests pass with core/sync validation. Frozen Shipping native acceptance, exact wind/reflection/occlusion restoration and a real 100-second animated tour pass. Source freeze `c8fd0ebc6b9fac467a7fa812bdc353c2c0e33a58`. Native art, movie and package use the same frozen executable. Windows CI remains pending; DX12 tone compiler errors and native stderr are retained on failure for diagnosis.

Reference parity and physical-display acceptance remain open (VIS 5/7).
