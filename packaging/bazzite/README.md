# Bazzite image integration

The preferred starting point for a custom Bazzite image is the current
[Universal Blue image template](https://github.com/ublue-os/image-template).
The template provides the bootc build, signing, publication, and upgrade
workflow that this small example deliberately omits.

Copy a release RPM into the template's build context. Add the equivalent of
`Containerfile.example` to the template build flow, keeping its recommended
cache and temporary mounts.

Before publication:

1. Pin the base image by digest.
2. Verify the release RPM checksum or signature.
3. Install the RPM using the package manager provided by the template.
4. Run `bootc container lint`.
5. Test boot, Fcitx restart, addon loading, helper activation, and removal on a
   disposable machine.

Choose the base image matching the target hardware. The example uses the KDE
desktop image for AMD and Intel graphics. Bazzite publishes separate NVIDIA
and handheld variants.
