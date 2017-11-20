#! /bin/bash
## vim:set ts=4 sw=4 et:
set -e; set -o pipefail

# Support for Travis CI -- https://travis-ci.org/upx/upx/builds
# Copyright (C) Markus Franz Xaver Johannes Oberhumer

#
# very first version of the upx-testsuite
#

if [[ $TRAVIS_OS_NAME == osx ]]; then
argv0=$0; argv0abs=$(greadlink -en -- "$0"); argv0dir=$(dirname "$argv0abs")
else
argv0=$0; argv0abs=$(readlink -en -- "$0"); argv0dir=$(dirname "$argv0abs")
fi
source "$argv0dir/travis_init.sh" || exit 1

if [[ $BM_T =~ (^|\+)SKIP($|\+) ]]; then
    echo "UPX testsuite SKIPPED."
    exit 0
fi
if [[ $BM_X == rebuild-stubs ]]; then
    exit 0
fi
[[ -f $upx_exe ]] && upx_exe=$(readlink -en -- "$upx_exe")

# create dirs
cd / || exit 1
mkbuilddirs $upx_testsuite_BUILDDIR
cd / && cd $upx_testsuite_BUILDDIR || exit 1
if [[ ! -d $upx_testsuite_SRCDIR/files/packed ]]; then exit 1; fi

# /***********************************************************************
# // support functions
# ************************************************************************/

testsuite_header() {
    print_header "$1"
}

testsuite_split_f() {
    fd=$(dirname "$1")
    fb=$(basename "$1")
    fsubdir=$(basename "$fd")
    # sanity checks
    if [[ ! -f "$1" || -z "$fsubdir" || -z "$fb" ]]; then
        fd= fb= fsubdir=
    fi
}

testsuite_check_sha() {
    (cd "$1" && sha256sum -b */* | LC_ALL=C sort -k2) > $1/.sha256sums.current
    echo
    cat $1/.sha256sums.current
    if ! cmp -s $1/.sha256sums.expected $1/.sha256sums.current; then
        echo "UPX-ERROR: $1 FAILED: checksum mismatch"
        diff -u $1/.sha256sums.expected $1/.sha256sums.current || true
        exit_code=1
        let num_errors+=1 || true
        all_errors="${all_errors} $1"
        #exit 1
    fi
    echo
}

testsuite_check_sha_decompressed() {
    (cd "$1" && sha256sum -b */* | LC_ALL=C sort -k2) > $1/.sha256sums.current
    if ! cmp -s $1/.sha256sums.expected $1/.sha256sums.current; then
        cat $1/.sha256sums.current
        echo "UPX-ERROR: FATAL: $1 FAILED: decompressed checksum mismatch"
        diff -u $1/.sha256sums.expected $1/.sha256sums.current || true
        exit 1
    fi
}

testsuite_use_canonicalized=1
testsuite_run_compress() {
    testsuite_header $testdir
    local files f
    if [[ $testsuite_use_canonicalized == 1 ]]; then
        files=t020_canonicalized/*/*
    else
        files=t010_decompressed/*/*
    fi
    for f in $files; do
        testsuite_split_f $f
        [[ -z $fb ]] && continue
        echo "# $f"
        mkdir -p $testdir/$fsubdir $testdir/.decompressed/$fsubdir
        $upx_run -qq --prefer-ucl "$@" $f -o $testdir/$fsubdir/$fb
        $upx_run -qq -d $testdir/$fsubdir/$fb -o $testdir/.decompressed/$fsubdir/$fb
    done
    testsuite_check_sha $testdir
    $upx_run -qq -l $testdir/*/*
    $upx_run -qq --file-info $testdir/*/*
    $upx_run -q -t $testdir/*/*
    if [[ $testsuite_use_canonicalized == 1 ]]; then
        # check that after decompression the file matches the canonicalized version
        cp t020_canonicalized/.sha256sums.expected $testdir/.decompressed/
        testsuite_check_sha_decompressed $testdir/.decompressed
        rm -rf "./$testdir/.decompressed"
    fi
}

# /***********************************************************************
# // expected checksums
# //
# // To ease maintenance of this script in case of updates this section
# // can be automatically re-created from the current checksums -
# // see call of function recreate_expected_sha256sums below.
# ************************************************************************/

recreate_expected_sha256sums() {
    local o="$1"
    local files f d
    echo "########## begin .sha256sums.recreate" > "$o"
    files=*/.sha256sums.current
    for f in $files; do
        d=$(dirname "$f")
        echo "expected_sha256sums__${d}="'"\' >> "$o"
        cat "$f" >> "$o"
        echo '"' >> "$o"
    done
    echo "########## end .sha256sums.recreate" >> "$o"
}

########## begin .sha256sums.recreate
expected_sha256sums__t010_decompressed="\
24158f78c34c4ef94bb7773a6dda7231d289be76c2f5f60e8b9ddb3f800c100e *amd64-linux.elf/upx-3.91
28d7ca8f0dfca8159e637eaf2057230b6e6719e07751aca1d19a45b5efed817c *arm-wince.pe/upx-3.91.exe
b1c1c38d50007616aaf8e942839648c80a6111023e0b411e5fa7a06c543aeb4a *armeb-linux.elf/upx-3.91
bcac77a287289301a45fde9a75e4e6c9ad7f8d57856bae6eafaae12ae4445a34 *i386-dos32.djgpp2.coff/upx-3.91.exe
730a513b72a094697f827e4ac1a4f8ef58a614fc7a7ad448fa58d60cd89af7ed *i386-linux.elf/upx-3.91
0dbc3c267ca8cd35ee3ea138b59c8b1ae35872c918be6d17df1a892b75710f9b *i386-win32.pe/upx-3.91.exe
8e5333ea040f5594d3e67d5b09e005d52b3a52ef55099a7c11d7e39ead38e66d *m68k-atari.tos/upx-3.91.ttp
c3f44b4d00a87384c03a6f9e7aec809c1addfe3e271244d38a474f296603088c *mipsel-linux.elf/upx-3.91
b8c35fa2956da17ca505956e9f5017bb5f3a746322647e24ccb8ff28059cafa4 *powerpc-linux.elf/upx-3.91
"
expected_sha256sums__t020_canonicalized="\
24158f78c34c4ef94bb7773a6dda7231d289be76c2f5f60e8b9ddb3f800c100e *amd64-linux.elf/upx-3.91
28d7ca8f0dfca8159e637eaf2057230b6e6719e07751aca1d19a45b5efed817c *arm-wince.pe/upx-3.91.exe
b1c1c38d50007616aaf8e942839648c80a6111023e0b411e5fa7a06c543aeb4a *armeb-linux.elf/upx-3.91
bcac77a287289301a45fde9a75e4e6c9ad7f8d57856bae6eafaae12ae4445a34 *i386-dos32.djgpp2.coff/upx-3.91.exe
730a513b72a094697f827e4ac1a4f8ef58a614fc7a7ad448fa58d60cd89af7ed *i386-linux.elf/upx-3.91
0dbc3c267ca8cd35ee3ea138b59c8b1ae35872c918be6d17df1a892b75710f9b *i386-win32.pe/upx-3.91.exe
8e5333ea040f5594d3e67d5b09e005d52b3a52ef55099a7c11d7e39ead38e66d *m68k-atari.tos/upx-3.91.ttp
c3f44b4d00a87384c03a6f9e7aec809c1addfe3e271244d38a474f296603088c *mipsel-linux.elf/upx-3.91
b8c35fa2956da17ca505956e9f5017bb5f3a746322647e24ccb8ff28059cafa4 *powerpc-linux.elf/upx-3.91
"
expected_sha256sums__t110_compress_ucl_nrv2b_3_no_filter="\
c82d400d99a78f21b594ad9900208ee89b3df5b67723c06dbb374dc3f42ca934 *amd64-linux.elf/upx-3.91
5b9ec916beae0eadc665235158a9ae5bce1309823a344503268a88e32e77824a *arm-wince.pe/upx-3.91.exe
a099aedddbaa960247209e08ed922b43f4bbd6559ad646a6ff801384e2f661eb *armeb-linux.elf/upx-3.91
960dc15876221832510142816605b9ef568c0de3050ca0a79f3553643c5d5e0f *i386-dos32.djgpp2.coff/upx-3.91.exe
43ec77282bbe73eb5d6f5471e9593127928bcfc2ebaedd93dd86969b12c9c61d *i386-linux.elf/upx-3.91
ca6925a15c1ab8931f0a8fe9ef87f5893403d6e46098f4cd1a5f6f6f0fbdeb44 *i386-win32.pe/upx-3.91.exe
14ff2a4e215a25ed7442b004bca3d82094f7c01784fc4876eb50d365441f35c3 *m68k-atari.tos/upx-3.91.ttp
4163afbb2475b669e131265c0f7ea179c35f19ccce66732feee08fe20c33775e *mipsel-linux.elf/upx-3.91
00c9d16157a734e1b386e5fe01dbae76c7f8ab7d6035dfa6975d656b43ef4e67 *powerpc-linux.elf/upx-3.91
"
expected_sha256sums__t120_compress_ucl_nrv2d_3_no_filter="\
27b530885ccb425cf14963feea677c4177cce5324821c277823b3262a7a6ced4 *amd64-linux.elf/upx-3.91
22216286b1bf3066d9022b921f37beff6712b5f3fc8c092f2dc1477638d9f8cc *arm-wince.pe/upx-3.91.exe
90e2960d01214b3e2f62f76effa2360fed655e72392acfa8096d1d9a729ade21 *armeb-linux.elf/upx-3.91
b6e98d36bd916fa63ec799e47dd7cac3674154370a9680492d84f1853bf14c3e *i386-dos32.djgpp2.coff/upx-3.91.exe
483e4a7eba0b88302658fcaacb7ee1de32fb514dac5dccb010a55a5202918c0c *i386-linux.elf/upx-3.91
d2692b3e4a278559456e299164714c4bb8ebbcf230ab12521619e2e94580597d *i386-win32.pe/upx-3.91.exe
cd1ae0f2781787bf7c61f3600cc889313e6027615d78e562d624d717671e55c3 *m68k-atari.tos/upx-3.91.ttp
26b30c10ba8980fa6fc564b79eb2931e07bb7b2f89f07a0656c28be71185a3ea *mipsel-linux.elf/upx-3.91
bbce9449d105e4ac7e9a04d56b286cbba3b34c1bf8ee928e9e97a4943ae3c5ac *powerpc-linux.elf/upx-3.91
"
expected_sha256sums__t130_compress_ucl_nrv2e_3_no_filter="\
81a528ed6f678bdbb1c18a9ee69b7d04169466e756a71baa98c7651ad1c40bec *amd64-linux.elf/upx-3.91
08c55815175ce0d34fca3b368336dd346a2354dbe4f046210c82f6961350a50f *arm-wince.pe/upx-3.91.exe
6dae3b18770e6af45e074b092c1f2f3632bef1996ce71af0f6e252caf34555a1 *armeb-linux.elf/upx-3.91
45f50d69e685f7ea752f76c05554d4c2ce023c0218465a4f8919138a76ae6c71 *i386-dos32.djgpp2.coff/upx-3.91.exe
f1db16f6be23b0bd96226e7e57f8b41ddfb93ede5ca0750ae680bbd615523083 *i386-linux.elf/upx-3.91
eb7c2f74979c11b35193a0a9d428596bda46420d9363666fe1b967f5cd1610c6 *i386-win32.pe/upx-3.91.exe
cefb13395220fb2e931d0fb32e27663c4a27035f9e79131bbabc44fa54e6336e *m68k-atari.tos/upx-3.91.ttp
602be188fca1dd63593a2ace68ea221d720fd2ddf9df85c1ab019feda69f1cad *mipsel-linux.elf/upx-3.91
2f299fdbc37bf6675559c5b1245aecf56de268bc074bd558bd7d9f84f7be5eae *powerpc-linux.elf/upx-3.91
"
expected_sha256sums__t140_compress_lzma_2_no_filter="\
12d2ec20370655396711fc794db41745c21f51c998b766e6aa96283feb00eb48 *amd64-linux.elf/upx-3.91
9759deb5aa8fb004c4b23bbe174042e45869aedeea1a1dd1b729be0e736814da *arm-wince.pe/upx-3.91.exe
9b25bcc69fcdbdf4b4adc54e5c6508749c918179644ff9fa34b437bc7e42eca5 *armeb-linux.elf/upx-3.91
a2a800d2ba5cfc1b6bb2b48c91adccb5d3c3b6c0b5c548affccac9244197a312 *i386-dos32.djgpp2.coff/upx-3.91.exe
1865bc9b76579e33df4038f3d0abf848993b0c865b687f4b4bd482099416ef80 *i386-linux.elf/upx-3.91
80aba41aad8268085e853ec872f885981838a625c14095d21ba70cb7abe045a5 *i386-win32.pe/upx-3.91.exe
bbed61e42fa7b330b5cde66e4614329f41e21facff1f3667edc03495219c29f9 *m68k-atari.tos/upx-3.91.ttp
57b47244a3a0d01725cbdc9af8572cbe20e2d173857015ad4d32245b52577dc2 *mipsel-linux.elf/upx-3.91
6b02b01fa48910a104b523b265503131e4259ef83765464d002614e1a4eba38f *powerpc-linux.elf/upx-3.91
"
expected_sha256sums__t150_compress_ucl_2_all_filters="\
3e0df832830a4a0cca77da16868a2655f85b2b36f783123b6f067f5586460704 *amd64-linux.elf/upx-3.91
c7b0f611e9941be58b700219e7a5d34cdbdbf972b6184b13dec5e98fe84de808 *arm-wince.pe/upx-3.91.exe
ef7c6febc18d5ecf7d846f38d75f398a92b5b21eb5d35b1eac4935d0e6de92ac *armeb-linux.elf/upx-3.91
425c9128285f49b41f9b736f48794f5bebba6981250f669e5a342016b89f2170 *i386-dos32.djgpp2.coff/upx-3.91.exe
d6cb6d50dfda98ff8efbab2d2c8751adfb67df58652313c630d45db1b89e921e *i386-linux.elf/upx-3.91
5565f8196d971feec261dc663ca7ec329fd82b1b18ad49593b865edbaa15765d *i386-win32.pe/upx-3.91.exe
78f24d77855034d467568f05c22cb5e3abd167c90a4d89f4e2059c3e6faa3e2b *m68k-atari.tos/upx-3.91.ttp
23d6856df8f31b9176e0f4709135ce81656ed94539bf909e873b787ce89821cd *mipsel-linux.elf/upx-3.91
8dbf63b21d3ced1cceec71e507d3086de2612b8ce8ba35ee2d75acd06435baf1 *powerpc-linux.elf/upx-3.91
"
expected_sha256sums__t160_compress_all_methods_1_no_filter="\
27a248133cd386332daa14ec25367b4f1ff5595df213a2d72c341bbb7f264033 *amd64-linux.elf/upx-3.91
6b2333719a4fe6c8d2067f682d57cf6fc5fd928bffad4e61aaffcc31287772a7 *arm-wince.pe/upx-3.91.exe
d094f3a0e80e579aa6afeac239aacb3d8210580acdb095c9fc5511d77da7aeaa *armeb-linux.elf/upx-3.91
d09af3652aa601650f9cd0f125d54e50dfe57b45b9871567140e62a04d032407 *i386-dos32.djgpp2.coff/upx-3.91.exe
110bb3344a27b2677085e130956371df43ca6c231c01070480bb3a9e519fed30 *i386-linux.elf/upx-3.91
c3c8b428f7e57a528db89f1365b4f3fda60f0dc03eadb30775ecdbadaa19f0aa *i386-win32.pe/upx-3.91.exe
53c77efbccf41072c4c206343ba3c838be04c47eab415d18c08f086d481612db *m68k-atari.tos/upx-3.91.ttp
616cefa819c0e516c554faac20e6aaf9350a2aacf0041a2641d5809c0da220df *mipsel-linux.elf/upx-3.91
04ad8f4a60de80bbba7953489d3ef4b74b89a3961ae764acee00bd6df814ed9a *powerpc-linux.elf/upx-3.91
"
expected_sha256sums__t170_compress_all_methods_no_lzma_5_no_filter="\
c8f0808d9cd81ece5f04c33388e6c5a056e2ceeb03308a1aa390ea6afc22d6ba *amd64-linux.elf/upx-3.91
685b7e419b8b0fe3cabdf338a5cad17da55edc608c1bb91c13580b5988d38908 *arm-wince.pe/upx-3.91.exe
7344cf818e8a668c7364f47779742bf41b451ebb7db85abc917c463a5f9c9d9a *armeb-linux.elf/upx-3.91
fd0652470c19ebb4a2d1a49e02e71acf9fadab78e513bb4f75d1dc26a0caa7a3 *i386-dos32.djgpp2.coff/upx-3.91.exe
3b9dd7de613541f567b2573063399473deeaa015a1c3679fe0fb339d80724e0d *i386-linux.elf/upx-3.91
5b334db8debd2d59470cad25c7b45e38f6195cdafe92dc8281e4edc9c51385ef *i386-win32.pe/upx-3.91.exe
db1c6a70d990cb9a8e02db9b28054267658ce371b8a50e909efdd04cd3670279 *m68k-atari.tos/upx-3.91.ttp
19b801ace68ca0ff034dd557d3ef0a8340d8f17d7b97f120727f67ce098a7d7a *mipsel-linux.elf/upx-3.91
a0b3ba40848df9b3a8aac31bc20671992be179ec2d702337bd1addd15513c2e4 *powerpc-linux.elf/upx-3.91
"
########## end .sha256sums.recreate

# /***********************************************************************
# // init
# ************************************************************************/

#set -x # debug
exit_code=0
num_errors=0
all_errors=

if [[ $BM_T =~ (^|\+)ALLOW_FAIL($|\+) ]]; then
    echo "ALLOW_FAIL"
    set +e
fi

[[ -z $upx_exe && -f $upx_BUILDDIR/upx.out ]] && upx_exe=$upx_BUILDDIR/upx.out
[[ -z $upx_exe && -f $upx_BUILDDIR/upx.exe ]] && upx_exe=$upx_BUILDDIR/upx.exe
if [[ -z $upx_exe ]]; then exit 1; fi
upx_run=$upx_exe
if [[ $BM_T =~ (^|\+)qemu($|\+) && -n $upx_qemu ]]; then
    upx_run="$upx_qemu $upx_qemu_flags -- $upx_exe"
fi
if [[ $BM_T =~ (^|\+)wine($|\+) && -n $upx_wine ]]; then
    upx_run="$upx_wine $upx_wine_flags $upx_exe"
fi
if [[ $BM_T =~ (^|\+)valgrind($|\+) ]]; then
    if [[ -z $upx_valgrind ]]; then
        upx_valgrind="valgrind"
    fi
    if [[ -z $upx_valgrind_flags ]]; then
        #upx_valgrind_flags="--leak-check=full --show-reachable=yes"
        #upx_valgrind_flags="-q --leak-check=no --error-exitcode=1"
        upx_valgrind_flags="--leak-check=no --error-exitcode=1"
    fi
    upx_run="$upx_valgrind $upx_valgrind_flags $upx_exe"
fi

if [[ $BM_B =~ (^|\+)coverage($|\+) ]]; then
    (cd / && cd $upx_BUILDDIR && lcov -d . --zerocounters)
fi

export UPX="--prefer-ucl --no-color --no-progress"

# let's go
if ! $upx_run --version;          then echo "UPX-ERROR: FATAL: upx --version FAILED"; exit 1; fi
if ! $upx_run -L >/dev/null 2>&1; then echo "UPX-ERROR: FATAL: upx -L FAILED"; exit 1; fi
if ! $upx_run --help >/dev/null;  then echo "UPX-ERROR: FATAL: upx --help FAILED"; exit 1; fi
rm -rf ./testsuite_1
mkbuilddirs testsuite_1
cd testsuite_1 || exit 1

# /***********************************************************************
# // decompression tests
# ************************************************************************/

testdir=t010_decompressed
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected

testsuite_header $testdir
for f in $upx_testsuite_SRCDIR/files/packed/*/upx-3.91*; do
    testsuite_split_f $f
    [[ -z $fb ]] && continue
    echo "# $f"
    mkdir -p $testdir/$fsubdir
    $upx_run -qq -d $f -o $testdir/$fsubdir/$fb
done
testsuite_check_sha $testdir


# run one pack+unpack step to canonicalize the files
testdir=t020_canonicalized
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected

testsuite_header $testdir
for f in t010_decompressed/*/*; do
    testsuite_split_f $f
    [[ -z $fb ]] && continue
    echo "# $f"
    mkdir -p $testdir/$fsubdir/.packed
    $upx_run -qq --prefer-ucl -1 $f -o $testdir/$fsubdir/.packed/$fb
    $upx_run -qq -d $testdir/$fsubdir/.packed/$fb -o $testdir/$fsubdir/$fb
done
testsuite_check_sha $testdir

# /***********************************************************************
# // compression tests
# // info: we use fast compression levels because we want to
# //   test UPX and not the compression libraries
# ************************************************************************/

testdir=t110_compress_ucl_nrv2b_3_no_filter
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected
time testsuite_run_compress --nrv2b -3 --no-filter

testdir=t120_compress_ucl_nrv2d_3_no_filter
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected
time testsuite_run_compress --nrv2d -3 --no-filter

testdir=t130_compress_ucl_nrv2e_3_no_filter
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected
time testsuite_run_compress --nrv2e -3 --no-filter

testdir=t140_compress_lzma_2_no_filter
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected
time testsuite_run_compress --lzma -2 --no-filter

testdir=t150_compress_ucl_2_all_filters
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected
time testsuite_run_compress -2 --all-filters

testdir=t160_compress_all_methods_1_no_filter
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected
time testsuite_run_compress --all-methods -1 --no-filter

testdir=t170_compress_all_methods_no_lzma_5_no_filter
mkdir $testdir; v=expected_sha256sums__$testdir; echo -n "${!v}" >$testdir/.sha256sums.expected
time testsuite_run_compress --all-methods --no-lzma -5 --no-filter

# /***********************************************************************
# // summary
# ************************************************************************/

# recreate checkums from current version for an easy update in case of changes
recreate_expected_sha256sums .sha256sums.recreate

testsuite_header "UPX testsuite summary"
if ! $upx_run --version; then
    echo "UPX-ERROR: upx --version FAILED"
    exit 1
fi
echo
echo "upx_exe='$upx_exe'"
if [[ "$upx_run" != "$upx_exe" ]]; then
    echo "upx_run='$upx_run'"
fi
if [[ -f "$upx_exe" ]]; then
    ls -l "$upx_exe"
    file "$upx_exe" || true
fi
echo "upx_testsuite_SRCDIR='$upx_testsuite_SRCDIR'"
echo "upx_testsuite_BUILDDIR='$upx_testsuite_BUILDDIR'"
echo ".sha256sums.{expected,current}:"
cat */.sha256sums.expected | LC_ALL=C sort | wc
cat */.sha256sums.current  | LC_ALL=C sort | wc
echo
if [[ $exit_code == 0 ]]; then
    echo "UPX testsuite passed. All done."
else
    echo "UPX-ERROR: UPX testsuite FAILED:${all_errors}"
    echo "UPX-ERROR: UPX testsuite FAILED with $num_errors error(s). See log file."
fi
exit $exit_code
