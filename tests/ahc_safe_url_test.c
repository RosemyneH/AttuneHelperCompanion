#include "ahc/ahc_safe_url.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

int main(void)
{
    assert(ahc_url_safe_for_download("https://github.com/a/b") == true);
    assert(ahc_url_safe_for_download("http://x") == false);
    assert(ahc_url_safe_for_download("https://evil.test/\"a") == false);
    assert(ahc_url_safe_for_download("https://a/b c") == false);
    assert(ahc_path_safe_for_arg("C:/temp/x") == true);
    assert(ahc_path_safe_for_arg("bad\"path") == false);
    char safe_line[] = "owner-repo-main/foo/Addon.toc";
    assert(ahc_zip_list_line_looks_dangerous(safe_line) == false);
    char bad1[] = "../foo";
    assert(ahc_zip_list_line_looks_dangerous(bad1) == true);
    char bad2[] = "/abs";
    assert(ahc_zip_list_line_looks_dangerous(bad2) == true);
    char not_bad[] = "foo..bar/ok.toc";
    assert(ahc_zip_list_line_looks_dangerous(not_bad) == false);
    const char *listing = "a/b\nc/d\n";
    assert(ahc_zip_listing_looks_dangerous(listing, strlen(listing)) == false);
    return 0;
}
