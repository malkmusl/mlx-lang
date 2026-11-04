# wget

`wget URL [-O FILE]`: the document at URL (http or https; redirects
followed) saved as FILE (the URL's last path part when none is given,
"index.html" when it has none), on std.http.client (std.net, std.tls).
It is built for Linux (`mlx-out/bin/wget`) and for brixOS
(`mlx-out/bin/brix/wget`, in the boot image: tools/check_brix_net.sh
fetches files from the host with it over the kernel's network stack).
