/*
 * elfcat - ELF visualizer. Generates HTML files from ELF binaries.
 *
 * Single-file C port of https://github.com/rbakbashev/elfcat (v0.1.10).
 * Uses only the C standard library (C99).
 *
 * Build:  cc -std=c99 -O2 -o elfcat elfcat.c -lm
 * Usage:  elfcat path/to/file      (writes <basename>.html to the CWD)
 *
 * Original work Copyright (c) Ruslan Akbashev, distributed under the Zlib license.
 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ELFCAT_VERSION "0.1.10"
#define DEFAULT_COLUMNS 16
#define INDENT "  "

/* ------------------------------------------------------------------------- */
/* Embedded assets                                                           */
/* ------------------------------------------------------------------------- */

/* Each asset is stored as a NULL-terminated array of lines. */

static const char *const STYLE_CSS[] = {
    "html {",
    "  font-family: monospace;",
    "}",
    "#headertable {",
    "  width: 100%;",
    "}",
    "#rightmenu {",
    "  vertical-align: top;",
    "  text-align: right;",
    "  float: right;",
    "}",
    "#credits {",
    "  color: #ddd;",
    "  text-decoration: none;",
    "  margin-bottom: 0.5em;",
    "  display: block;",
    "}",
    "#credits:hover {",
    "  color: #000;",
    "}",
    "#arrow_opacity_range {",
    "  width: 8em;",
    "}",
    ".right_hidden {",
    "  text-align: left;",
    "  display: none;",
    "}",
    ".textbutton {",
    "  color: #222;",
    "  cursor: pointer;",
    "  background: none;",
    "  border: none;",
    "  padding: 0;",
    "  margin: 0;",
    "  margin-bottom: 0.5em;",
    "}",
    ".textbutton::before {",
    "  content: \"[\";",
    "}",
    ".textbutton::after {",
    "  content: \"]\";",
    "}",
    ".textbutton:hover {",
    "  color: #922;",
    "}",
    ".textbutton:active {",
    "  color: #d11;",
    "}",
    ".textbutton:hover::before {",
    "  color: #222;",
    "}",
    ".textbutton:hover::after {",
    "  color: #222;",
    "}",
    "label, input {",
    "  vertical-align: middle;",
    "}",
    ".legend_rect {",
    "  float: left;",
    "  margin-right: 0.4em;",
    "  width: 3em;",
    "  height: 1.5em;",
    "  border: 1px solid rgba(0, 0, 0, .2);",
    "}",
    "ul {",
    "  list-style: none;",
    "  padding: 0;",
    "}",
    "#help > p {",
    "  margin-top: 0;",
    "  margin-bottom: 0.5em;",
    "  width: 29em;",
    "}",
    "#help > ul {",
    "  margin-top: 0;",
    "}",
    "#help > p + p {",
    "  margin-top: 1em;",
    "}",
    "li {",
    "  clear: left;",
    "  height: 2em;",
    "  display: flex;",
    "  flex-direction: row;",
    "  align-items: center;",
    "}",
    ".number {",
    "  text-decoration: underline dotted #888;",
    "}",
    "#desc {",
    "  width: 250px;",
    "}",
    ".infotables {",
    "  padding: 0;",
    "  display: block;",
    "}",
    ".conceal {",
    "  display: none;",
    "  border: 1px solid #000;",
    "  max-width: 600px;",
    "  margin-bottom: 1em;",
    "}",
    ".conceal div {",
    "  word-break: break-word;",
    "}",
    ".indirect .phdr_itable::after {",
    "  content: \" (references this segment)\";",
    "}",
    ".indirect .shdr_itable::after {",
    "  content: \" (references this section)\";",
    "}",
    ".phdr_itable::before {",
    "  content: \"Program header\";",
    "}",
    ".shdr_itable::before {",
    "  content: \"Section header\";",
    "}",
    ".phdr_itable {",
    "  text-align: left;",
    "}",
    ".shdr_itable {",
    "  text-align: left;",
    "}",
    ".segment_itable::before {",
    "  content: \"Segment\";",
    "}",
    ".section_itable::before {",
    "  content: \"Section\";",
    "}",
    ".segment_itable {",
    "  text-align: left;",
    "}",
    ".section_itable {",
    "  text-align: left;",
    "}",
    "#offsets {",
    "  display: inline-block;",
    "  text-align: right;",
    "  white-space: pre;",
    "}",
    "#bytes {",
    "  border: 1px solid;",
    "  display: inline-block;",
    "  word-break: break-word;",
    "  /* 16 bytes * (2 chars per byte + 1 space) - 1 final space */",
    "  width: 47ch;",
    "  white-space: pre;",
    "}",
    "#ascii {",
    "  border: 1px solid;",
    "  display: inline-block;",
    "  width: 16ch;",
    "  white-space: pre;",
    "}",
    "#vmap {",
    "  border: 1px solid;",
    "  display: inline-block;",
    "  width: 47ch;",
    "  vertical-align: top;",
    "  position: sticky;",
    "  top: 8px;",
    "}",
    "#sticky_table {",
    "  display: inline-block;",
    "  vertical-align: top;",
    "  position: sticky;",
    "  top: 8px;",
    "}",
    "#desc, .itable {",
    "  vertical-align: top;",
    "}",
    ".hover:hover {",
    "  background-color: #ee9;",
    "}",
    "",
    ".ident {",
    "  background-color: #e99;",
    "}",
    "/* highlight fields */",
    ".ident:hover > span {",
    "  background-color: #ebb;",
    "}",
    "/* Duplicate of .hover:hover with more specifity. Used for elements like `#magic', for whom",
    " * rule `.ident:hover > span' would override rule `.hover:hover' */",
    ".ident:hover > span:hover {",
    "  background-color: #ee9;",
    "}",
    "",
    "/* same for .ehdr, but we also don't want to highlight .ident */",
    ".ehdr {",
    "  background-color: #99e;",
    "}",
    ".ehdr:hover > span:not(.ident) {",
    "  background-color: #bbe;",
    "}",
    ".ehdr:hover > span:not(.ident):hover {",
    "  background-color: #ee9;",
    "}",
    "",
    ".phdr {",
    "  background-color: #eb9;",
    "}",
    ".phdr:hover > span {",
    "  background-color: #ed9;",
    "}",
    ".phdr:hover > span:hover {",
    "  background-color: #ef9;",
    "}",
    "",
    ".shdr {",
    "  background-color: #9be;",
    "}",
    ".shdr:hover > span {",
    "  background-color: #9de;",
    "}",
    ".shdr:hover > span:hover {",
    "  background-color: #9fe;",
    "}",
    "",
    ".segment {",
    "  background-color: #f99;",
    "}",
    ".section {",
    "  background-color: #f9f;",
    "}",
    "/* there are only sections inside segments due to code */",
    ".segment > .section, .segm_sect_legend {",
    "  background: repeating-linear-gradient(",
    "    -45deg,",
    "    #f99,",
    "    #f99 10px,",
    "    #f9f 10px,",
    "    #f9f 20px",
    "  );",
    "}",
    "/* another duplicate of .hover:hover with more specifity */",
    ".segment > .section:hover {",
    "  background: initial;",
    "  background-color: #ee9;",
    "}",
    "",
    ".segment > .segment_subrange:hover {",
    "  background: initial;",
    "  background-color: #f59;",
    "}",
    "",
    "svg {",
    "  position: absolute;",
    "  left: 0;",
    "  right: 0;",
    "  pointer-events: none;",
    "  overflow: visible;",
    "}",
    NULL,
};

static const char *const HIGHLIGHT_JS[] = {
    "let color = \"#ee9\";",
    "",
    "function addPairHighlighting(elem, depElem) {",
    "    elem.addEventListener(\"mouseenter\", function(event) {",
    "        event.target.style.backgroundColor = color;",
    "        depElem.style.backgroundColor = color;",
    "    }, false);",
    "",
    "    elem.addEventListener(\"mouseleave\", function(event) {",
    "        event.target.style.backgroundColor = \"\";",
    "        depElem.style.backgroundColor = \"\";",
    "    }, false);",
    "}",
    "",
    "function highlightClasses(primaryClass, secondaryClass) {",
    "    let primaryElems = document.getElementsByClassName(primaryClass);",
    "    let secondaryElems = document.getElementsByClassName(secondaryClass);",
    "",
    "    if (primaryElems.length === 0 || secondaryElems.length === 0) {",
    "        return;",
    "    }",
    "",
    "    let primaryElem = primaryElems[0];",
    "    let secondaryElem = secondaryElems[0];",
    "",
    "    addPairHighlighting(primaryElem, secondaryElem);",
    "    addPairHighlighting(secondaryElem, primaryElem);",
    "}",
    "",
    NULL,
};

static const char *const DESCRIPTION_JS[] = {
    "let label = document.getElementById('desc')",
    "let descriptions = {",
    "    ehdr:         \"ELF file header (ehdr)\",",
    "    ident:        \"Identifier (e_ident)\",",
    "    magic:        \"Magic (EI_MAG0 .. EI_MAG3)\",",
    "    class:        \"Object file class (EI_CLASS)\",",
    "    data:         \"Data encoding (EI_DATA)\",",
    "    ver:          \"File version, should be 1 (EI_VERSION)\",",
    "    abi:          \"ABI (EI_OSABI)\",",
    "    abi_ver:      \"ABI version, should be 0 for SysV (EI_ABIVERSION)\",",
    "    pad:          \"Identifier padding\",",
    "    e_type:       \"Object file type (e_type)\",",
    "    e_machine:    \"Object file type (e_machine)\",",
    "    e_version:    \"File version, should be 1 (e_version)\",",
    "    e_entry:      \"Entrypoint vaddr (e_entry)\",",
    "    e_phoff:      \"Offset in file to program header table (e_phoff)\",",
    "    e_shoff:      \"Offset in file to section header table (e_shoff)\",",
    "    e_flags:      \"Processor-specific flags (e_flags)\",",
    "    e_ehsize:     \"Size of ELF file header (e_ehsize)\",",
    "    e_phentsize:  \"Size of a program header table entry (e_phentsize)\",",
    "    e_phnum:      \"Number of program header table entries (e_phnum)\",",
    "    e_shentsize:  \"Size of a section header table entry (e_shentsize)\",",
    "    e_shnum:      \"Number of section header table entries (e_shnum)\",",
    "    e_shstrndx:   \"What section is a string table (e_shstrndx)\",",
    "    phdr:         \"Program header\",",
    "    p_type:       \"Segment type (p_type)\",",
    "    p_flags:      \"Segment flags (p_flags)\",",
    "    p_offset:     \"Offset in file (p_offset)\",",
    "    p_vaddr:      \"Virtual address in memory (p_vaddr)\",",
    "    p_paddr:      \"Reserved for physical address in memory (p_paddr)\",",
    "    p_filesz:     \"Size of segment in file (p_filesz)\",",
    "    p_memsz:      \"Size of segment in memory (p_memsz)\",",
    "    p_align:      \"Alignment (p_align)\",",
    "    segment:      \"Segment\",",
    "    shdr:         \"Section header\",",
    "    sh_name:      \"Offset to the section name string table containing this section name (sh_name)\",",
    "    sh_type:      \"Section type (sh_type)\",",
    "    sh_flags:     \"Section flags (sh_flags)\",",
    "    sh_addr:      \"Virtual address in memory. Should be 0 if not put into memory (sh_addr)\",",
    "    sh_offset:    \"Offset in file (sh_offset)\",",
    "    sh_size:      \"Section size. Also size in file unless type is SHT_NOBITS (sh_size)\",",
    "    sh_link:      \"Section index of an associated section (sh_link)\",",
    "    sh_info:      \"Additional information about section (sh_info)\",",
    "    sh_addralign: \"Address alignment of the section (sh_addralign)\",",
    "    sh_entsize:   \"Size of each entry if section has table of fixed-size entries (sh_entsize)\",",
    "    section:      \"Section\",",
    "    section_in_segment: \"Section in segment\",",
    "}",
    "let separator = \"<br>&#x2193<br>\";",
    "",
    "function stripFileInfoPrefix(str) {",
    "    return str.replace(\"fileinfo_\", \"\");",
    "}",
    "",
    "function stripInfoPrefix(str) {",
    "    return str.replace(\"info_\", \"\");",
    "}",
    "",
    "function stripBinPrefix(str) {",
    "    return str.replace(\"bin_\", \"\");",
    "}",
    "",
    "function getDesc(cl) {",
    "    if (cl === \"\") {",
    "        return null;",
    "    }",
    "",
    "    if (descriptions[cl] !== undefined) {",
    "        return descriptions[cl];",
    "    }",
    "",
    "    if (descriptions[stripFileInfoPrefix(cl)] !== undefined) {",
    "        return descriptions[stripFileInfoPrefix(cl)];",
    "    }",
    "",
    "    if (descriptions[stripInfoPrefix(cl)] !== undefined) {",
    "        return descriptions[stripInfoPrefix(cl)];",
    "    }",
    "",
    "    if (descriptions[stripBinPrefix(cl)] !== undefined) {",
    "        return descriptions[stripBinPrefix(cl)];",
    "    }",
    "",
    "    return null;",
    "}",
    "",
    "function iterateParents(el) {",
    "    let txt = \"\";",
    "    let keywords = [];",
    "",
    "    do {",
    "        let classList = el.classList;",
    "        if (classList !== undefined) {",
    "            for (let i = 0; i < classList.length; ++i) {",
    "                keywords.push(classList[i]);",
    "            }",
    "        }",
    "",
    "        el = el.parentNode;",
    "    } while (el !== null && el.tagName !== \"HTML\")",
    "",
    "    // fix all occurences where segments and sections overlap.",
    "    // once again we are relying on programming by which sections are always inside segments.",
    "    // also, this is O(n^2) ugly.",
    "    let i = 1;",
    "    while (i < keywords.length) {",
    "        if (keywords[i] !== 'segment') {",
    "            ++i;",
    "            continue;",
    "        }",
    "",
    "        let have_section_before = false;",
    "        let j = 0;",
    "        for (; j < i; ++j) {",
    "            if (keywords[j] == 'section') {",
    "                have_section_before = true;",
    "                break;",
    "            }",
    "        }",
    "",
    "        if (have_section_before) {",
    "            keywords[i] = 'section_in_segment';",
    "            keywords.splice(j, 1);",
    "        } else {",
    "            ++i;",
    "        }",
    "    }",
    "",
    "    for (let i = 0; i < keywords.length; ++i) {",
    "        let keyword = keywords[i];",
    "        let desc = getDesc(keyword);",
    "",
    "        if (desc !== null) {",
    "            if (txt === \"\") {",
    "                txt = desc;",
    "            } else {",
    "                txt = desc + separator + txt;",
    "            }",
    "        }",
    "    }",
    "",
    "    return txt;",
    "}",
    "",
    "document.addEventListener(\"mouseover\", function (e) {",
    "    let event = e || window.event;",
    "",
    "    let target = event.target || event.srcElement;",
    "",
    "    if (!target.classList.contains('legend_rect')) {",
    "        document.getElementById('desc').innerHTML = iterateParents(target);",
    "    }",
    "}, false);",
    NULL,
};

static const char *const CONCEAL_JS[] = {
    "let prevShownTables = [];",
    "",
    "function getFirstClass(el) {",
    "    let cls = el.classList[0];",
    "",
    "    if (cls === undefined) {",
    "        return \"\";",
    "    } else {",
    "        return cls;",
    "    }",
    "}",
    "",
    "function listOfParents(el) {",
    "    let list = [getFirstClass(el)];",
    "",
    "    while (el.tagName !== \"HTML\") {",
    "        el = el.parentNode;",
    "",
    "        list.unshift(getFirstClass(el));",
    "    }",
    "",
    "    return list;",
    "}",
    "",
    "function hidePreviousTables() {",
    "    for (let i = 0; i < prevShownTables.length; ++i) {",
    "        prevShownTables[i].style.display = \"none\";",
    "        prevShownTables[i].classList.remove(\"indirect\");",
    "    }",
    "}",
    "",
    "function showIfMatches(cl, prefix, newPrefix, indirect = false) {",
    "    if (!cl.startsWith(prefix)) {",
    "        return;",
    "    }",
    "",
    "    let tableId = cl.replace(prefix, newPrefix);",
    "    let table = document.getElementById(tableId);",
    "",
    "    if (table === null) {",
    "        return;",
    "    }",
    "",
    "    table.style.display = \"block\";",
    "",
    "    if (indirect) {",
    "        table.classList.add(\"indirect\");",
    "    } else {",
    "        table.classList.remove(\"indirect\");",
    "    }",
    "",
    "    prevShownTables.push(table);",
    "}",
    "",
    "document.addEventListener(\"mouseover\", function (e) {",
    "    let event = e || window.event;",
    "    let target = event.target || event.srcElement;",
    "    let parents = listOfParents(target);",
    "    let cleared = false;",
    "    const prefix = 'bin_';",
    "",
    "    for (let i = 0; i < parents.length; i++) {",
    "        let cl = parents[i];",
    "",
    "        if (!cl.startsWith(prefix)) {",
    "            continue;",
    "        }",
    "",
    "        if (!cleared) {",
    "            cleared = true;",
    "            hidePreviousTables();",
    "        }",
    "",
    "        showIfMatches(cl, prefix, \"info_\");",
    "        showIfMatches(cl, \"bin_segment\", \"info_phdr\", true);",
    "        showIfMatches(cl, \"bin_section\", \"info_shdr\", true);",
    "    }",
    "}, false);",
    NULL,
};

static const char *const OFFSETS_JS[] = {
    "function populateOffsets(columns) {",
    "    let rows = Math.ceil(fileLen / columns);",
    "    let elements = \"\";",
    "    let offset = 0;",
    "",
    "    for (let i = 0; i < rows; ++i) {",
    "        elements += offset.toString(16) + \"</br>\\n\";",
    "        offset += columns;",
    "    }",
    "",
    "    document.getElementById('offsets').innerHTML = elements;",
    "}",
    NULL,
};

static const char *const ARROWS_JS[] = {
    "const arrows = document.getElementById('arrows');",
    "let connections = [];",
    "let batchElems = '';",
    "",
    "function getAbsPosition(elem) {",
    "    let x = 0, y = 0;",
    "",
    "    do {",
    "        x += elem.offsetLeft || 0;",
    "        y += elem.offsetTop || 0;",
    "",
    "        elem = elem.offsetParent;",
    "    } while (elem);",
    "",
    "    return {",
    "        x: x,",
    "        y: y",
    "    };",
    "}",
    "",
    "function getBoundingBoxSizes(elem) {",
    "    let rect = elem.getBoundingClientRect();",
    "",
    "    return {",
    "        w: rect.width || elem.offsetWidth,",
    "        h: rect.height || elem.offsetHeight",
    "    };",
    "}",
    "",
    "function addSvgArrow(elem1, elem2) {",
    "    let off1 = getAbsPosition(elem1);",
    "    let off2 = getAbsPosition(elem2);",
    "",
    "    let bb1 = getBoundingBoxSizes(elem1);",
    "",
    "    let x1 = off1.x + bb1.w / 2;",
    "    let y1 = off1.y;",
    "",
    "    let x2 = off2.x;",
    "    let y2 = off2.y;",
    "",
    "    batchElems += '<line x1=\"' + x1 + '\" y1=\"' + y1 + '\" '",
    "                      + 'x2=\"' + x2 + '\" y2=\"' + y2 + '\"/>';",
    "}",
    "",
    "function jumpToElem(elem) {",
    "    elem.scrollIntoView();",
    "}",
    "",
    "function setJumpCallback(elemFrom, elemTo) {",
    "    elemFrom.addEventListener(\"click\", function(e) {",
    "        jumpToElem(elemTo);",
    "    }, true);",
    "}",
    "",
    "function connect(sel1, sel2) {",
    "    let elem1 = document.querySelector(sel1);",
    "    let elem2 = document.querySelector(sel2);",
    "",
    "    if (elem1 === null || elem2 === null) {",
    "        return;",
    "    }",
    "",
    "    connections.push([elem1, elem2]);",
    "",
    "    addSvgArrow(elem1, elem2);",
    "",
    "    setJumpCallback(elem1, elem2);",
    "    setJumpCallback(elem2, elem1);",
    "}",
    "",
    "function clearArrows() {",
    "    batchElems = '';",
    "",
    "    arrows.innerHTML = '';",
    "}",
    "",
    "function pushArrowElems() {",
    "    arrows.innerHTML = batchElems;",
    "}",
    "",
    "function redrawArrows() {",
    "    clearArrows();",
    "",
    "    for (let i = 0; i < connections.length; ++i) {",
    "        let conn = connections[i];",
    "",
    "        addSvgArrow(conn[0], conn[1]);",
    "    }",
    "",
    "    pushArrowElems();",
    "}",
    "",
    "window.onresize = function() {",
    "    redrawArrows();",
    "}",
    "",
    NULL,
};

static const char *const SETTINGS_JS[] = {
    "const settings = document.getElementById('settings');",
    "const help = document.getElementById('help');",
    "const arrow_input = document.getElementById('arrow_opacity_range');",
    "/* const arrows is reused from arrows.js */",
    "",
    "function toggleVisibility(elem) {",
    "    if (elem.style.display === \"none\" || elem.style.display === \"\") {",
    "        elem.style.display = \"block\";",
    "    } else {",
    "        elem.style.display = \"none\";",
    "    }",
    "}",
    "",
    "document.getElementById('settings_toggle').onclick = function() {",
    "    toggleVisibility(settings);",
    "",
    "    help.style.display = \"none\";",
    "}",
    "",
    "document.getElementById('help_toggle').onclick = function() {",
    "    toggleVisibility(help);",
    "",
    "    settings.style.display = \"none\";",
    "}",
    "",
    "function setArrowOpacity(e) {",
    "    arrows.style.opacity = e.target.valueAsNumber / 100;",
    "}",
    "",
    "arrow_input.addEventListener('change', setArrowOpacity);",
    "arrow_input.addEventListener('input', setArrowOpacity);",
    NULL,
};

/* ------------------------------------------------------------------------- */
/* ELF definitions                                                           */
/* ------------------------------------------------------------------------- */

#define ELF_EI_CLASS 4
#define ELF_EI_DATA 5
#define ELF_EI_VERSION 6
#define ELF_EI_OSABI 7
#define ELF_EI_ABIVERSION 8
#define ELF_EI_NIDENT 16

#define ELF_CLASS32 1
#define ELF_CLASS64 2

#define ELF_DATA2LSB 1
#define ELF_DATA2MSB 2

#define ELF_EV_CURRENT 1

#define ELF_OSABI_SYSV 0
#define ELF_OSABI_HPUX 1

#define ELF_ET_NONE 0
#define ELF_ET_REL 1
#define ELF_ET_EXEC 2
#define ELF_ET_DYN 3
#define ELF_ET_CORE 4
#define ELF_ET_LOOS 0xfe00
#define ELF_ET_HIOS 0xfeff
#define ELF_ET_LOPROC 0xff00
#define ELF_ET_HIPROC 0xffff

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_TLS 7
#define PT_LOOS 0x60000000u
#define PT_GNU_EH_FRAME 0x6474e550u
#define PT_GNU_STACK 0x6474e551u
#define PT_GNU_RELRO 0x6474e552u
#define PT_HIOS 0x6fffffffu
#define PT_LOPROC 0x70000000u
#define PT_HIPROC 0x7fffffffu

#define PF_X 1u
#define PF_W 2u
#define PF_R 4u

#define NT_GNU_BUILD_ID 3

#define SHN_UNDEF 0

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_INIT_ARRAY 14
#define SHT_FINI_ARRAY 15
#define SHT_LOOS 0x60000000u
#define SHT_GNU_HASH 0x6ffffff6u
#define SHT_VER_NEED 0x6ffffffeu
#define SHT_HIOS 0x6fffffffu
#define SHT_LOPROC 0x70000000u
#define SHT_HIPROC 0x7fffffffu

#define SHF_WRITE 1u
#define SHF_ALLOC 2u
#define SHF_EXECINSTR 4u

/* Scratch buffer for the "Unknown: N" fallbacks. */
static char unknown_buf[48];

static const char *type_to_string(uint16_t t)
{
    switch (t) {
    case ELF_ET_NONE: return "None (NONE)";
    case ELF_ET_REL: return "Relocatable object file (REL)";
    case ELF_ET_EXEC: return "Executable file (EXEC)";
    case ELF_ET_DYN: return "Shared object file (DYN)";
    case ELF_ET_CORE: return "Core file (CORE)";
    case ELF_ET_LOOS: case ELF_ET_HIOS: return "Environment-specific use";
    case ELF_ET_LOPROC: case ELF_ET_HIPROC: return "Processor-specific use";
    }
    snprintf(unknown_buf, sizeof unknown_buf, "Unknown %u", (unsigned)t);
    return unknown_buf;
}

static const char *abi_to_string(uint8_t abi)
{
    switch (abi) {
    case ELF_OSABI_SYSV: return "SysV";
    case ELF_OSABI_HPUX: return "HP-UX";
    case 2: return "NetBSD";
    case 3: return "Linux";
    case 4: return "Hurd";
    case 6: return "Solaris";
    case 7: return "AIX";
    case 8: return "IRIX";
    case 9: return "FreeBSD";
    case 10: return "Tru64";
    case 11: return "Modesto";
    case 12: return "OpenBSD";
    case 13: return "OpenVMS";
    case 255: return "Standalone";
    }
    snprintf(unknown_buf, sizeof unknown_buf, "Unknown: %u", (unsigned)abi);
    return unknown_buf;
}

static const char *machine_to_string(uint16_t m)
{
    switch (m) {
    case 0: return "None";
    case 1: return "AT&T WE 32100";
    case 2: return "SPARC";
    case 3: return "x86";
    case 4: return "Motorolla 68000";
    case 5: return "Motorolla 88000";
    case 6: return "Intel MCU";
    case 7: return "Intel 80860";
    case 8: return "MIPS";
    case 9: return "IBM System/370";
    case 10: return "MIPS RS3000 little-endian";
    case 14: return "HP PA-RISC";
    case 19: return "Intel 80960";
    case 20: return "PowerPC";
    case 21: return "PowerPC 64-bit";
    case 22: return "S390";
    case 40: return "ARM Aarch32";
    case 50: return "Itanium IA-64";
    case 62: return "x86-64";
    case 183: return "ARM Aarch64";
    case 190: return "CUDA";
    case 224: return "AMDGPU";
    case 243: return "RISC-V";
    }
    snprintf(unknown_buf, sizeof unknown_buf, "Unknown: %u", (unsigned)m);
    return unknown_buf;
}

static const char *ptype_to_string(uint32_t p)
{
    switch (p) {
    case PT_NULL: return "NULL";
    case PT_LOAD: return "LOAD";
    case PT_DYNAMIC: return "DYNAMIC";
    case PT_INTERP: return "INTERP";
    case PT_NOTE: return "NOTE";
    case PT_SHLIB: return "SHLIB";
    case PT_PHDR: return "PHDR";
    case PT_TLS: return "TLS";
    case PT_LOOS: return "LOOS";
    case PT_GNU_EH_FRAME: return "GNU_EH_FRAME (OS-specific)";
    case PT_GNU_STACK: return "GNU_STACK (OS-specific)";
    case PT_GNU_RELRO: return "GNU_RELRO (OS-specific)";
    case PT_HIOS: return "HIOS";
    case PT_LOPROC: return "LOPROC";
    case PT_HIPROC: return "HIPROC";
    }
    snprintf(unknown_buf, sizeof unknown_buf, "Unknown: %" PRIu32, p);
    return unknown_buf;
}

static void pflags_to_string(uint32_t flags, char out[4])
{
    int n = 0;
    if (flags & PF_R) out[n++] = 'R';
    if (flags & PF_W) out[n++] = 'W';
    if (flags & PF_X) out[n++] = 'X';
    out[n] = '\0';
}

static const char *shtype_to_string(uint32_t t)
{
    switch (t) {
    case SHT_NULL: return "NULL";
    case SHT_PROGBITS: return "PROGBITS";
    case SHT_SYMTAB: return "SYMTAB";
    case SHT_STRTAB: return "STRTAB";
    case SHT_RELA: return "RELA";
    case SHT_HASH: return "HASH";
    case SHT_DYNAMIC: return "DYNAMIC";
    case SHT_NOTE: return "NOTE";
    case SHT_NOBITS: return "NOBITS";
    case SHT_REL: return "REL";
    case SHT_SHLIB: return "SHLIB";
    case SHT_INIT_ARRAY: return "INIT_ARRAY";
    case SHT_FINI_ARRAY: return "FINI_ARRAY";
    case SHT_DYNSYM: return "DYNSYM";
    case SHT_LOOS: return "LOOS";
    case SHT_GNU_HASH: return "GNU_HASH (OS-specific)";
    case SHT_VER_NEED: return "VER_NEED (OS-specific)";
    case SHT_HIOS: return "HIOS";
    case SHT_LOPROC: return "LOPROC";
    case SHT_HIPROC: return "HIPROC";
    }
    snprintf(unknown_buf, sizeof unknown_buf, "Unknown: %" PRIu32, t);
    return unknown_buf;
}

static void shflags_to_string(uint64_t flags, char out[4])
{
    int n = 0;
    if (flags & SHF_WRITE) out[n++] = 'W';
    if (flags & SHF_ALLOC) out[n++] = 'A';
    if (flags & SHF_EXECINSTR) out[n++] = 'X';
    if (n == 0) out[n++] = '0';
    out[n] = '\0';
}

/* ------------------------------------------------------------------------- */
/* Utilities                                                                 */
/* ------------------------------------------------------------------------- */

static void die(const char *fmt, ...)
{
    va_list ap;
    fputs("Failed to ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void *xrealloc(void *p, size_t n)
{
    void *r = realloc(p, n ? n : 1);
    if (!r)
        die("allocate memory");
    return r;
}

static char *xasprintf(const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        die("format string");

    s = xrealloc(NULL, (size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

/* Grow-by-doubling dynamic array helper. */
#define VEC_PUSH(arr, len, cap, val)                                     \
    do {                                                                 \
        if ((len) == (cap)) {                                            \
            (cap) = (cap) ? (cap) * 2 : 8;                               \
            (arr) = xrealloc((arr), (cap) * sizeof *(arr));              \
        }                                                                \
        (arr)[(len)++] = (val);                                          \
    } while (0)

static int is_ascii_graphic(uint8_t b)
{
    return b >= 0x21 && b <= 0x7e;
}

static void human_format_bytes(uint64_t bytes, char *out, size_t outsz)
{
    static const char prefixes[] = "KMGTPE";
    unsigned exponent;

    if (bytes < 1024) {
        snprintf(out, outsz, "%" PRIu64 " B", bytes);
        return;
    }

    exponent = (unsigned)(log((double)bytes) / log(1024.0));
    if (exponent < 1)
        exponent = 1;
    if (exponent > 6)
        exponent = 6;

    snprintf(out, outsz, "%.1f %ciB",
             (double)bytes / (double)(UINT64_C(1) << (10 * exponent)),
             prefixes[exponent - 1]);
}

static const char *html_escape(uint8_t ch)
{
    switch (ch) {
    case '&': return "&amp;";
    case '<': return "&lt;";
    case '>': return "&gt;";
    case '"': return "&quot;";
    case '\'': return "&apos;";
    }
    return NULL;
}

/* Strict UTF-8 validation (same acceptance rules as Rust's str::from_utf8). */
static int utf8_valid(const uint8_t *s, size_t n)
{
    size_t i = 0;

    while (i < n) {
        uint8_t c = s[i];
        size_t need;
        uint8_t lo = 0x80, hi = 0xbf;

        if (c < 0x80) { i++; continue; }
        else if (c >= 0xc2 && c <= 0xdf) need = 1;
        else if (c == 0xe0) { need = 2; lo = 0xa0; }
        else if (c >= 0xe1 && c <= 0xec) need = 2;
        else if (c == 0xed) { need = 2; hi = 0x9f; }
        else if (c >= 0xee && c <= 0xef) need = 2;
        else if (c == 0xf0) { need = 3; lo = 0x90; }
        else if (c >= 0xf1 && c <= 0xf3) need = 3;
        else if (c == 0xf4) { need = 3; hi = 0x8f; }
        else return 0;

        if (n - i - 1 < need)
            return 0;
        if (s[i + 1] < lo || s[i + 1] > hi)
            return 0;
        for (size_t k = 2; k <= need; k++)
            if (s[i + k] < 0x80 || s[i + k] > 0xbf)
                return 0;
        i += need + 1;
    }
    return 1;
}

/* Equivalent of Rust's Path::file_name() for typical inputs. */
static char *basename_dup(const char *path)
{
    size_t len = strlen(path);
    size_t start;
    char *out;

    for (;;) {
        while (len > 1 && path[len - 1] == '/')
            len--;
        if (len >= 2 && path[len - 1] == '.' && path[len - 2] == '/')
            len -= 2;
        else
            break;
    }

    start = len;
    while (start > 0 && path[start - 1] != '/')
        start--;

    if (len == start)
        return NULL;
    if (len - start == 1 && path[start] == '.')
        return NULL;
    if (len - start == 2 && path[start] == '.' && path[start + 1] == '.')
        return NULL;

    out = xrealloc(NULL, len - start + 1);
    memcpy(out, path + start, len - start);
    out[len - start] = '\0';
    return out;
}

/* ------------------------------------------------------------------------- */
/* Parsed ELF representation                                                 */
/* ------------------------------------------------------------------------- */

typedef enum {
    R_IDENT,
    R_FILE_HEADER,
    R_HEADER_FIELD,
    R_PROGRAM_HEADER,
    R_SECTION_HEADER,
    R_PHDR_FIELD,
    R_SHDR_FIELD,
    R_SEGMENT,
    R_SECTION,
    R_SEGMENT_SUBRANGE
} range_kind;

typedef struct {
    uint8_t kind;
    uint32_t idx;
    const char *field;
    int32_t next;
} range_node;

/*
 * For every byte of the file: an ordered list of ranges that start there
 * (singly linked through a node pool) and the number of ranges ending there.
 */
typedef struct {
    size_t size;
    int32_t *head;
    int32_t *tail;
    uint32_t *ends;
    range_node *nodes;
    size_t nnodes, capnodes;
} ranges_t;

typedef struct {
    const char *cls;
    const char *desc;
    char *value;
} info_t;

typedef struct {
    uint32_t ptype;
    char flags[4];
    uint64_t file_offset, file_size, vaddr, memsz, alignment;
} phdr_t;

typedef struct {
    uint64_t name;
    uint32_t shtype;
    uint64_t flags, addr, file_offset, size, link, info, addralign, entsize;
} shdr_t;

typedef struct {
    const uint8_t *name;
    size_t namesz;
    const uint8_t *desc;
    size_t descsz;
    uint32_t ntype;
} note_t;

typedef struct {
    const uint8_t *strings;
    size_t size;
} strtab_t;

typedef struct {
    const char *filename;
    const uint8_t *contents;
    size_t file_size;
    int little_endian;

    info_t *info;
    size_t ninfo, capinfo;

    ranges_t ranges;

    phdr_t *phdrs;
    size_t nphdrs, capphdrs;

    shdr_t *shdrs;
    size_t nshdrs, capshdrs;

    uint16_t shstrndx;
    strtab_t shnstrtab;

    note_t *notes;
    size_t nnotes, capnotes;
} elf_t;

static void ranges_init(ranges_t *r, size_t size)
{
    memset(r, 0, sizeof *r);
    r->size = size;
    r->head = xrealloc(NULL, size * sizeof *r->head);
    r->tail = xrealloc(NULL, size * sizeof *r->tail);
    r->ends = calloc(size ? size : 1, sizeof *r->ends);
    if (!r->ends)
        die("allocate memory");
    for (size_t i = 0; i < size; i++)
        r->head[i] = r->tail[i] = -1;
}

/* Ranges that do not fit inside the file are ignored (the original panics). */
static void add_range(ranges_t *r, uint64_t start, uint64_t len, range_kind kind,
                      uint32_t idx, const char *field)
{
    range_node node;
    int32_t id;

    if (len == 0 || start >= r->size || len > r->size - start)
        return;
    if (r->nnodes >= INT32_MAX)
        die("allocate memory");

    node.kind = (uint8_t)kind;
    node.idx = idx;
    node.field = field;
    node.next = -1;
    id = (int32_t)r->nnodes;
    VEC_PUSH(r->nodes, r->nnodes, r->capnodes, node);

    if (r->tail[start] < 0)
        r->head[start] = id;
    else
        r->nodes[r->tail[start]].next = id;
    r->tail[start] = id;

    r->ends[start + len - 1]++;
}

static void push_info(elf_t *elf, const char *cls, const char *desc, char *value)
{
    info_t it;
    it.cls = cls;
    it.desc = desc;
    it.value = value;
    VEC_PUSH(elf->info, elf->ninfo, elf->capinfo, it);
}

static uint64_t read_uint(const uint8_t *p, unsigned size, int le)
{
    uint64_t v = 0;
    for (unsigned i = 0; i < size; i++) {
        unsigned shift = le ? i : (size - 1 - i);
        v |= (uint64_t)p[i] << (8 * shift);
    }
    return v;
}

static const char *strtab_get(const strtab_t *t, uint64_t idx)
{
    static char *buf;
    static size_t bufcap;

    for (uint64_t end = idx; end < t->size; end++) {
        if (t->strings[end] == 0) {
            size_t n = (size_t)(end - idx);
            if (!utf8_valid(t->strings + idx, n))
                return "";
            if (n + 1 > bufcap) {
                bufcap = n + 1;
                buf = xrealloc(buf, bufcap);
            }
            memcpy(buf, t->strings + idx, n);
            buf[n] = '\0';
            return buf;
        }
    }
    return "";
}

/* Field layout tables: name, offset, size */
typedef struct {
    const char *name;
    unsigned off, size;
} field_t;

enum {
    E_TYPE, E_MACHINE, E_VERSION, E_ENTRY, E_PHOFF, E_SHOFF, E_FLAGS,
    E_EHSIZE, E_PHENTSIZE, E_PHNUM, E_SHENTSIZE, E_SHNUM, E_SHSTRNDX, E_COUNT
};

static const field_t ehdr32_fields[E_COUNT] = {
    {"e_type", 16, 2}, {"e_machine", 18, 2}, {"e_version", 20, 4},
    {"e_entry", 24, 4}, {"e_phoff", 28, 4}, {"e_shoff", 32, 4},
    {"e_flags", 36, 4}, {"e_ehsize", 40, 2}, {"e_phentsize", 42, 2},
    {"e_phnum", 44, 2}, {"e_shentsize", 46, 2}, {"e_shnum", 48, 2},
    {"e_shstrndx", 50, 2},
};

static const field_t ehdr64_fields[E_COUNT] = {
    {"e_type", 16, 2}, {"e_machine", 18, 2}, {"e_version", 20, 4},
    {"e_entry", 24, 8}, {"e_phoff", 32, 8}, {"e_shoff", 40, 8},
    {"e_flags", 48, 4}, {"e_ehsize", 52, 2}, {"e_phentsize", 54, 2},
    {"e_phnum", 56, 2}, {"e_shentsize", 58, 2}, {"e_shnum", 60, 2},
    {"e_shstrndx", 62, 2},
};

/* Listed in on-disk order for each class. */
static const field_t phdr32_fields[8] = {
    {"p_type", 0, 4}, {"p_offset", 4, 4}, {"p_vaddr", 8, 4},
    {"p_paddr", 12, 4}, {"p_filesz", 16, 4}, {"p_memsz", 20, 4},
    {"p_flags", 24, 4}, {"p_align", 28, 4},
};

static const field_t phdr64_fields[8] = {
    {"p_type", 0, 4}, {"p_flags", 4, 4}, {"p_offset", 8, 8},
    {"p_vaddr", 16, 8}, {"p_paddr", 24, 8}, {"p_filesz", 32, 8},
    {"p_memsz", 40, 8}, {"p_align", 48, 8},
};

enum {
    SH_NAME, SH_TYPE, SH_FLAGS, SH_ADDR, SH_OFFSET, SH_SIZE,
    SH_LINK, SH_INFO, SH_ADDRALIGN, SH_ENTSIZE, SH_COUNT
};

static const field_t shdr32_fields[SH_COUNT] = {
    {"sh_name", 0, 4}, {"sh_type", 4, 4}, {"sh_flags", 8, 4},
    {"sh_addr", 12, 4}, {"sh_offset", 16, 4}, {"sh_size", 20, 4},
    {"sh_link", 24, 4}, {"sh_info", 28, 4}, {"sh_addralign", 32, 4},
    {"sh_entsize", 36, 4},
};

static const field_t shdr64_fields[SH_COUNT] = {
    {"sh_name", 0, 4}, {"sh_type", 4, 4}, {"sh_flags", 8, 8},
    {"sh_addr", 16, 8}, {"sh_offset", 24, 8}, {"sh_size", 32, 8},
    {"sh_link", 40, 4}, {"sh_info", 44, 4}, {"sh_addralign", 48, 8},
    {"sh_entsize", 56, 8},
};

static uint64_t phdr_field(const uint8_t *p, const field_t *fields, const char *name, int le)
{
    for (int i = 0; i < 8; i++)
        if (strcmp(fields[i].name, name) == 0)
            return read_uint(p + fields[i].off, fields[i].size, le);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Parser                                                                    */
/* ------------------------------------------------------------------------- */

static void push_file_info(elf_t *elf)
{
    char human[32];

    push_info(elf, "file_name", "File name", xasprintf("%s", elf->filename));

    if (elf->file_size < 1024) {
        push_info(elf, "file_size", "File size", xasprintf("%zu B", elf->file_size));
    } else {
        human_format_bytes(elf->file_size, human, sizeof human);
        push_info(elf, "file_size", "File size",
                  xasprintf("%s (%zu B)", human, elf->file_size));
    }
}

static int push_ident_info(elf_t *elf, char **err)
{
    const uint8_t *b = elf->contents;
    uint8_t class = b[ELF_EI_CLASS], data = b[ELF_EI_DATA];
    uint8_t version = b[ELF_EI_VERSION], abi = b[ELF_EI_OSABI];
    uint8_t abi_ver = b[ELF_EI_ABIVERSION];

    if (class == ELF_CLASS32)
        push_info(elf, "class", "Object class", xasprintf("32-bit"));
    else if (class == ELF_CLASS64)
        push_info(elf, "class", "Object class", xasprintf("64-bit"));
    else {
        *err = xasprintf("Unknown bitness: %u", (unsigned)class);
        return 0;
    }

    if (data == ELF_DATA2LSB)
        push_info(elf, "data", "Data encoding", xasprintf("Little endian"));
    else if (data == ELF_DATA2MSB)
        push_info(elf, "data", "Data encoding", xasprintf("Big endian"));
    else {
        *err = xasprintf("Unknown endianness: %u", (unsigned)data);
        return 0;
    }

    if (version != ELF_EV_CURRENT)
        push_info(elf, "ver", "Uncommon version(!)", xasprintf("%u", (unsigned)version));

    push_info(elf, "abi", abi == ELF_OSABI_SYSV ? "ABI" : "Uncommon ABI(!)",
              xasprintf("%s", abi_to_string(abi)));

    if (!(abi == ELF_OSABI_SYSV && abi_ver == 0)) {
        push_info(elf, "abi_ver",
                  (abi == ELF_OSABI_SYSV && abi_ver != 0) ? "Uncommon ABI version(!)"
                                                          : "ABI version",
                  xasprintf("%u", (unsigned)abi_ver));
    }

    return 1;
}

static void push_ehdr_info(elf_t *elf, const uint64_t *e)
{
    push_info(elf, "e_type", "Type", xasprintf("%s", type_to_string((uint16_t)e[E_TYPE])));
    push_info(elf, "e_machine", "Architecture",
              xasprintf("%s", machine_to_string((uint16_t)e[E_MACHINE])));
    push_info(elf, "e_entry", "Entrypoint",
              xasprintf("<span class='number' title='%" PRIu64 "'>0x%" PRIx64 "</span>",
                        e[E_ENTRY], e[E_ENTRY]));
    push_info(elf, "ph", "Program headers",
              xasprintf("<span title='0x%" PRIx64 "' class='number fileinfo_e_phnum'>%" PRIu64
                        "</span> * "
                        "<span title='0x%" PRIx64 "' class='number fileinfo_e_phentsize'>%" PRIu64
                        "</span> @ "
                        "<span title='0x%" PRIx64 "' class='number fileinfo_e_phoff'>%" PRIu64
                        "</span>",
                        e[E_PHNUM], e[E_PHNUM], e[E_PHENTSIZE], e[E_PHENTSIZE],
                        e[E_PHOFF], e[E_PHOFF]));
    push_info(elf, "sh", "Section headers",
              xasprintf("<span title='0x%" PRIx64 "' class='number fileinfo_e_shnum'>%" PRIu64
                        "</span> * "
                        "<span title='0x%" PRIx64 "' class='number fileinfo_e_shentsize'>%" PRIu64
                        "</span> @ "
                        "<span title='0x%" PRIx64 "' class='number fileinfo_e_shoff'>%" PRIu64
                        "</span>",
                        e[E_SHNUM], e[E_SHNUM], e[E_SHENTSIZE], e[E_SHENTSIZE],
                        e[E_SHOFF], e[E_SHOFF]));
    if (e[E_FLAGS] != 0)
        push_info(elf, "e_flags", "Flags", xasprintf("0x%" PRIx64, e[E_FLAGS]));
}

static int parse_elfxx(elf_t *elf, int is64, char **err)
{
    const uint8_t *buf = elf->contents;
    size_t len = elf->file_size;
    int le = elf->little_endian;
    const field_t *ef = is64 ? ehdr64_fields : ehdr32_fields;
    const field_t *pf = is64 ? phdr64_fields : phdr32_fields;
    const field_t *sf = is64 ? shdr64_fields : shdr32_fields;
    size_t ehsize = is64 ? 64 : 52;
    size_t phsize = is64 ? 56 : 32;
    size_t shsize = is64 ? 64 : 40;
    uint64_t e[E_COUNT];
    uint64_t start;

    if (len < ehsize) {
        *err = xasprintf("file is smaller than ELF file header");
        return 0;
    }

    for (int i = 0; i < E_COUNT; i++)
        e[i] = read_uint(buf + ef[i].off, ef[i].size, le);

    elf->shstrndx = (uint16_t)e[E_SHSTRNDX];

    /* File header */
    push_ehdr_info(elf, e);
    add_range(&elf->ranges, 0, e[E_EHSIZE], R_FILE_HEADER, 0, NULL);
    for (int i = 0; i < E_COUNT; i++)
        add_range(&elf->ranges, ef[i].off, ef[i].size, R_HEADER_FIELD, 0, ef[i].name);

    /* Program headers */
    start = e[E_PHOFF];
    for (uint64_t i = 0; i < e[E_PHNUM]; i++) {
        const uint8_t *p;
        phdr_t ph;

        if (start > len || phsize > len - start) {
            *err = xasprintf("failed to read program header: out of file bounds");
            return 0;
        }
        p = buf + start;

        ph.ptype = (uint32_t)phdr_field(p, pf, "p_type", le);
        pflags_to_string((uint32_t)phdr_field(p, pf, "p_flags", le), ph.flags);
        ph.file_offset = phdr_field(p, pf, "p_offset", le);
        ph.file_size = phdr_field(p, pf, "p_filesz", le);
        ph.vaddr = phdr_field(p, pf, "p_vaddr", le);
        ph.memsz = phdr_field(p, pf, "p_memsz", le);
        ph.alignment = phdr_field(p, pf, "p_align", le);

        if (ph.file_offset != 0 && ph.file_size != 0)
            add_range(&elf->ranges, ph.file_offset, ph.file_size, R_SEGMENT, (uint32_t)i, NULL);

        add_range(&elf->ranges, start, phsize, R_PROGRAM_HEADER, (uint32_t)i, NULL);

        for (int f = 0; f < 8; f++)
            add_range(&elf->ranges, start + pf[f].off, pf[f].size, R_PHDR_FIELD, 0, pf[f].name);

        VEC_PUSH(elf->phdrs, elf->nphdrs, elf->capphdrs, ph);
        start += phsize;
    }

    /* Section headers */
    start = e[E_SHOFF];
    for (uint64_t i = 0; i < e[E_SHNUM]; i++) {
        const uint8_t *p;
        uint64_t v[SH_COUNT];
        shdr_t sh;

        if (start > len || shsize > len - start) {
            *err = xasprintf("failed to read section header: out of file bounds");
            return 0;
        }
        p = buf + start;

        for (int f = 0; f < SH_COUNT; f++)
            v[f] = read_uint(p + sf[f].off, sf[f].size, le);

        sh.name = v[SH_NAME];
        sh.shtype = (uint32_t)v[SH_TYPE];
        sh.flags = v[SH_FLAGS];
        sh.addr = v[SH_ADDR];
        sh.file_offset = v[SH_OFFSET];
        sh.size = v[SH_SIZE];
        sh.link = v[SH_LINK];
        sh.info = v[SH_INFO];
        sh.addralign = v[SH_ADDRALIGN];
        sh.entsize = v[SH_ENTSIZE];

        if (sh.file_offset != 0 && sh.size != 0 && sh.shtype != SHT_NOBITS)
            add_range(&elf->ranges, sh.file_offset, sh.size, R_SECTION, (uint32_t)i, NULL);

        add_range(&elf->ranges, start, shsize, R_SECTION_HEADER, (uint32_t)i, NULL);

        for (int f = 0; f < SH_COUNT; f++)
            add_range(&elf->ranges, start + sf[f].off, sf[f].size, R_SHDR_FIELD, 0, sf[f].name);

        VEC_PUSH(elf->shdrs, elf->nshdrs, elf->capshdrs, sh);
        start += shsize;
    }

    return 1;
}

static void add_ident_ranges(elf_t *elf)
{
    ranges_t *r = &elf->ranges;

    add_range(r, 0, ELF_EI_NIDENT, R_IDENT, 0, NULL);
    add_range(r, 0, 4, R_HEADER_FIELD, 0, "magic");
    add_range(r, 4, 1, R_HEADER_FIELD, 0, "class");
    add_range(r, 5, 1, R_HEADER_FIELD, 0, "data");
    add_range(r, 6, 1, R_HEADER_FIELD, 0, "ver");
    add_range(r, 7, 1, R_HEADER_FIELD, 0, "abi");
    add_range(r, 8, 1, R_HEADER_FIELD, 0, "abi_ver");
    add_range(r, 9, 7, R_HEADER_FIELD, 0, "pad");
}

static int in_file(const elf_t *elf, uint64_t off, uint64_t size)
{
    return off <= elf->file_size && size <= elf->file_size - off;
}

static void parse_string_tables(elf_t *elf)
{
    size_t idx = elf->shstrndx;

    if (elf->shstrndx != SHN_UNDEF && idx < elf->nshdrs) {
        const shdr_t *sh = &elf->shdrs[idx];
        if (in_file(elf, sh->file_offset, sh->size)) {
            elf->shnstrtab.strings = elf->contents + sh->file_offset;
            elf->shnstrtab.size = (size_t)sh->size;
        }
    }
}

/* Notes may come from segments or sections; "area" covers both. */
static void parse_note_area(elf_t *elf, uint64_t area_start, uint64_t area_size)
{
    const uint8_t *area;
    uint64_t start = 0;
    int le = elf->little_endian;

    if (!in_file(elf, area_start, area_size))
        return;
    area = elf->contents + area_start;

    while (start < area_size) {
        uint64_t avail = area_size - start;
        const uint8_t *p = area + start;
        uint64_t namesz, descsz, len;
        note_t note;

        if (avail < 12)
            break;

        namesz = read_uint(p, 4, le);
        descsz = read_uint(p + 4, 4, le);
        note.ntype = (uint32_t)read_uint(p + 8, 4, le);

        if (12 + namesz > avail || 12 + namesz + descsz > avail)
            break;

        note.name = p + 12;
        note.namesz = (size_t)namesz;
        note.desc = p + 12 + namesz;
        note.descsz = (size_t)descsz;

        len = 12 + namesz + descsz;
        while (len % 4 != 0)
            len++;

        add_range(&elf->ranges, area_start + start, len, R_SEGMENT_SUBRANGE, 0, NULL);
        VEC_PUSH(elf->notes, elf->nnotes, elf->capnotes, note);
        start += len;
    }
}

static void parse_notes(elf_t *elf)
{
    for (size_t i = 0; i < elf->nphdrs; i++)
        if (elf->phdrs[i].ptype == PT_NOTE)
            parse_note_area(elf, elf->phdrs[i].file_offset, elf->phdrs[i].file_size);
}

static int parse_elf(elf_t *elf, const char *filename, const uint8_t *buf, size_t len,
                     char **err)
{
    memset(elf, 0, sizeof *elf);
    elf->filename = filename;
    elf->contents = buf;
    elf->file_size = len;

    if (len < ELF_EI_NIDENT) {
        *err = xasprintf("file is smaller than ELF header's e_ident");
        return 0;
    }

    if (buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') {
        *err = xasprintf("mismatched magic: not an ELF file");
        return 0;
    }

    elf->little_endian = buf[ELF_EI_DATA] == ELF_DATA2LSB;
    ranges_init(&elf->ranges, len);

    push_file_info(elf);

    if (!push_ident_info(elf, err))
        return 0;

    if (!parse_elfxx(elf, buf[ELF_EI_CLASS] != ELF_CLASS32, err))
        return 0;

    add_ident_ranges(elf);
    parse_string_tables(elf);
    parse_notes(elf);

    return 1;
}

/* ------------------------------------------------------------------------- */
/* HTML output helpers                                                       */
/* ------------------------------------------------------------------------- */

static void put_indent(FILE *o, int level)
{
    for (int i = 0; i < level; i++)
        fputs(INDENT, o);
}

/* Write an indented line (with trailing newline). */
static void w(FILE *o, int level, const char *fmt, ...)
{
    va_list ap;
    put_indent(o, level);
    va_start(ap, fmt);
    vfprintf(o, fmt, ap);
    va_end(ap);
    fputc('\n', o);
}

/* Write indented text without a trailing newline. */
static void wnonl(FILE *o, int level, const char *fmt, ...)
{
    va_list ap;
    put_indent(o, level);
    va_start(ap, fmt);
    vfprintf(o, fmt, ap);
    va_end(ap);
}

/* Two-cell table row: "<tr> <td>LHS:</td> <td>" ... "</td> </tr>" */
static void row_begin(FILE *o, int level, const char *lhs)
{
    wnonl(o, level, "<tr> <td>%s:</td> <td>", lhs);
}

static void row_end(FILE *o)
{
    fputs("</td> </tr>\n", o);
}

static void wrow(FILE *o, int level, const char *lhs, const char *rhs)
{
    row_begin(o, level, lhs);
    fputs(rhs, o);
    row_end(o);
}

static void put_hex_dual(FILE *o, uint64_t v)
{
    fprintf(o, "<span class='number' title='%" PRIu64 "'>0x%" PRIx64 "</span>", v, v);
}

static void put_dec_dual(FILE *o, uint64_t v)
{
    fprintf(o, "<span class='number' title='0x%" PRIx64 "'>%" PRIu64 "</span>", v, v);
}

static void put_size_dual(FILE *o, uint64_t v)
{
    if (v >= 1024) {
        char human[32];
        human_format_bytes(v, human, sizeof human);
        fprintf(o, "<span class='number' title='0x%" PRIx64 "'>%" PRIu64 " (%s)</span>",
                v, v, human);
    } else {
        put_dec_dual(o, v);
    }
}

/* Emit an embedded asset (array of lines), indenting each non-empty line. */
static void indent_lines(FILE *o, const char *const *lines, int level)
{
    for (; *lines; lines++) {
        if (**lines) {
            put_indent(o, level);
            fputs(*lines, o);
        }
        fputc('\n', o);
    }
}

static void put_hex_byte(FILE *o, uint8_t b)
{
    static const char digits[] = "0123456789abcdef";
    fputc(digits[b >> 4], o);
    fputc(digits[b & 0xf], o);
}

static void put_string_slice(FILE *o, const uint8_t *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (is_ascii_graphic(s[i])) {
            fputc(s[i], o);
        } else {
            fputs("<b>", o);
            put_hex_byte(o, s[i]);
            fputs("</b> ", o);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Report generation                                                         */
/* ------------------------------------------------------------------------- */

static void generate_head(FILE *o, const elf_t *elf, const char *title)
{
    (void)elf;
    w(o, 1, "<head>");
    w(o, 2, "<meta charset='utf-8'>");
    w(o, 2, "<meta name='viewport' content='%s'>", "width=900, initial-scale=1");
    w(o, 2, "<title>%s</title>", title);
    w(o, 2, "<style>");
    indent_lines(o, STYLE_CSS, 3);
    w(o, 2, "</style>");
    w(o, 1, "</head>");
}

static void generate_svg_element(FILE *o)
{
    w(o, 2, "<svg width='100%%' height='100%%'>");
    w(o, 3, "<defs>");
    wnonl(o, 4, "<marker id='arrowhead' viewBox='0 0 10 10' ");
    wnonl(o, 0, "refX='10' refY='5' ");
    w(o, 0, "markerWidth='10' markerHeight='10' orient='auto'>");
    w(o, 5, "<path d='M 0 0 L 10 5 L 0 10 z' />");
    w(o, 4, "</marker>");
    w(o, 3, "</defs>");
    wnonl(o, 3, "<g id='arrows' stroke='black' ");
    wnonl(o, 0, "stroke-width='1' marker-end='url(#arrowhead)'>");
    w(o, 0, "</g>");
    w(o, 2, "</svg>");
}

static void generate_file_info_table(FILE *o, const elf_t *elf)
{
    w(o, 2, "<table>");
    for (size_t i = 0; i < elf->ninfo; i++) {
        wnonl(o, 3, "<tr class='fileinfo_%s'> ", elf->info[i].cls);
        wnonl(o, 0, "<td>%s:</td> ", elf->info[i].desc);
        wnonl(o, 0, "<td>%s</td> ", elf->info[i].value);
        w(o, 0, "</tr>");
    }
    w(o, 2, "</table>");
}

static void generate_help(FILE *o)
{
    static const char *const legend[][2] = {
        {"ident", "ELF Identification"},
        {"ehdr", "ELF Header"},
        {"phdr", "Program Header"},
        {"shdr", "Section Header"},
        {"segment", "Segment"},
        {"section", "Section"},
        {"segm_sect_legend", "Segment &amp; Section overlap"},
    };
    static const char help_text[] =
        "The leftmost column shows offsets within the file. "
        "The middle column is the file dump. It has ELF structs, sections and segments "
        "highlighted. Some fields that reference areas in the file are clickable and "
        "connected with arrows. The rightmost column shows printable ASCII characters "
        "corresponding to the file bytes.";

    w(o, 4, "<p>%s</p>", help_text);
    w(o, 4, "<p>Legend</p>");
    w(o, 4, "<ul>");
    for (size_t i = 0; i < sizeof legend / sizeof legend[0]; i++)
        w(o, 5, "<li><span class='legend_rect %s'></span>%s</li>", legend[i][0], legend[i][1]);
    w(o, 4, "</ul>");
}

static void generate_right_menu(FILE *o)
{
    w(o, 3, "<a id='credits' href='%s'>generated with elfcat %s</a>",
      "https://github.com/rbakbashev/elfcat", ELFCAT_VERSION);
    w(o, 3, "<button class='textbutton' id='settings_toggle'>Settings</button>");
    w(o, 3, "<button class='textbutton' id='help_toggle'>Help</button>");
    w(o, 3, "<div class='right_hidden' id='settings'>");
    w(o, 4, "<label for='arrow_opacity_range'>Arrow opacity:</label>");
    w(o, 4, "<input type='range' id='arrow_opacity_range' min='0' max='100' value='100'>");
    w(o, 3, "</div>");
    w(o, 3, "<div class='right_hidden' id='help'>");
    generate_help(o);
    w(o, 3, "</div>");
}

static void generate_phdr_info_tables(FILE *o, const elf_t *elf)
{
    for (size_t i = 0; i < elf->nphdrs; i++) {
        const phdr_t *ph = &elf->phdrs[i];

        w(o, 5, "<table class='conceal itable' id='info_phdr%zu'>", i);
        w(o, 5, "<th colspan='2' class='phdr_itable'></th>");

        wrow(o, 6, "Type", ptype_to_string(ph->ptype));
        wrow(o, 6, "Flags", ph->flags);
        row_begin(o, 6, "Offset in file"); put_hex_dual(o, ph->file_offset); row_end(o);
        row_begin(o, 6, "Size in file"); put_size_dual(o, ph->file_size); row_end(o);
        row_begin(o, 6, "Vaddr in memory"); put_hex_dual(o, ph->vaddr); row_end(o);
        row_begin(o, 6, "Size in memory"); put_size_dual(o, ph->memsz); row_end(o);
        row_begin(o, 6, "Alignment"); put_hex_dual(o, ph->alignment); row_end(o);

        w(o, 5, "</table>");
    }
}

static void generate_shdr_info_tables(FILE *o, const elf_t *elf)
{
    for (size_t i = 0; i < elf->nshdrs; i++) {
        const shdr_t *sh = &elf->shdrs[i];
        char flags[4];

        shflags_to_string(sh->flags, flags);

        w(o, 5, "<table class='conceal itable' id='info_shdr%zu'>", i);
        w(o, 5, "<th colspan='2' class='shdr_itable'></th>");

        row_begin(o, 6, "Index"); fprintf(o, "%zu", i); row_end(o);
        wrow(o, 6, "Name", strtab_get(&elf->shnstrtab, sh->name));
        wrow(o, 6, "Type", shtype_to_string(sh->shtype));
        wrow(o, 6, "Flags", flags);
        row_begin(o, 6, "Vaddr in memory"); put_hex_dual(o, sh->addr); row_end(o);
        row_begin(o, 6, "Offset in file"); put_hex_dual(o, sh->file_offset); row_end(o);
        row_begin(o, 6, "Size in file"); put_size_dual(o, sh->size); row_end(o);
        row_begin(o, 6, "Linked section"); fprintf(o, "%" PRIu64, sh->link); row_end(o);
        row_begin(o, 6, "Extra info"); put_dec_dual(o, sh->info); row_end(o);
        row_begin(o, 6, "Alignment"); put_hex_dual(o, sh->addralign); row_end(o);
        row_begin(o, 6, "Size of entries"); put_size_dual(o, sh->entsize); row_end(o);

        w(o, 5, "</table>");
    }
}

static void generate_note_data(FILE *o, const note_t *note)
{
    row_begin(o, 6, "Name");
    if (note->namesz > 0)
        put_string_slice(o, note->name, note->namesz - 1);
    row_end(o);

    row_begin(o, 6, "Type"); fprintf(o, "0x%" PRIx32, note->ntype); row_end(o);

    if (note->ntype == NT_GNU_BUILD_ID) {
        row_begin(o, 6, "Build ID");
        for (size_t i = 0; i < note->descsz; i++)
            put_hex_byte(o, note->desc[i]);
        row_end(o);
    } else {
        row_begin(o, 6, "Desc");
        put_string_slice(o, note->desc, note->descsz);
        row_end(o);
    }
}

static void generate_segment_detail(FILE *o, const elf_t *elf, const phdr_t *ph)
{
    if (ph->ptype == PT_INTERP) {
        row_begin(o, 6, "Interpreter");
        if (ph->file_size > 0 && in_file(elf, ph->file_offset, ph->file_size))
            put_string_slice(o, elf->contents + ph->file_offset, (size_t)ph->file_size - 1);
        row_end(o);
    } else if (ph->ptype == PT_NOTE) {
        /* As in the original, every note is listed under every NOTE segment. */
        for (size_t i = 0; i < elf->nnotes; i++) {
            generate_note_data(o, &elf->notes[i]);
            if (i != elf->nnotes - 1)
                w(o, 6, "<tr> <td><br></td> </tr>");
        }
    }
}

static void generate_strtab_data(FILE *o, const uint8_t *section, size_t size)
{
    size_t curr_start = 0;

    w(o, 6, "<tr>");
    w(o, 7, "<td></td>");
    w(o, 7, "<td>");
    w(o, 8, "<div>");

    /* Faithfully mirrors the original's string splitting logic. */
    for (size_t i = 0; i < size; i++) {
        if (section[i] == 0) {
            size_t end = curr_start == 0 ? 0 : i - 1;
            size_t n = end + 1 - curr_start;

            if (utf8_valid(section + curr_start, n) && section[curr_start] != 0) {
                put_indent(o, 9);
                fwrite(section + curr_start, 1, n, o);
                fputc('\n', o);
            }
            curr_start = i + 1;
        }
    }

    w(o, 8, "</div>");
    w(o, 7, "</td>");
    w(o, 6, "</tr>");
}

static int has_segment_detail(uint32_t ptype)
{
    return ptype == PT_INTERP || ptype == PT_NOTE;
}

static int has_section_detail(uint32_t shtype)
{
    return shtype == SHT_STRTAB;
}

static void generate_segment_info_tables(FILE *o, const elf_t *elf)
{
    for (size_t i = 0; i < elf->nphdrs; i++) {
        const phdr_t *ph = &elf->phdrs[i];

        w(o, 5, "<table class='conceal itable' id='info_segment%zu'>", i);
        w(o, 5, "<th colspan='2' class='segment_itable'></th>");

        wrow(o, 6, "Type", ptype_to_string(ph->ptype));
        row_begin(o, 6, "Size in file"); put_size_dual(o, ph->file_size); row_end(o);
        row_begin(o, 6, "Size in memory"); put_size_dual(o, ph->memsz); row_end(o);

        if (has_segment_detail(ph->ptype)) {
            w(o, 6, "<tr><td><br></td></tr>");
            generate_segment_detail(o, elf, ph);
        }

        w(o, 5, "</table>");
    }
}

static void generate_section_info_tables(FILE *o, const elf_t *elf)
{
    for (size_t i = 0; i < elf->nshdrs; i++) {
        const shdr_t *sh = &elf->shdrs[i];

        w(o, 5, "<table class='conceal itable' id='info_section%zu'>", i);
        w(o, 5, "<th colspan='2' class='section_itable'></th>");

        wrow(o, 6, "Type", shtype_to_string(sh->shtype));
        row_begin(o, 6, "Size"); put_size_dual(o, sh->size); row_end(o);

        if (has_section_detail(sh->shtype)) {
            w(o, 6, "<tr><td><br></td></tr>");
            if (in_file(elf, sh->file_offset, sh->size))
                generate_strtab_data(o, elf->contents + sh->file_offset, (size_t)sh->size);
        }

        w(o, 5, "</table>");
    }
}

static void generate_sticky_info_tables(FILE *o, const elf_t *elf)
{
    w(o, 2, "<table id='sticky_table' cellspacing='0'>");
    w(o, 3, "<tr>");
    w(o, 4, "<td id='desc'></td>");

    w(o, 4, "<td class='infotables'>");
    generate_phdr_info_tables(o, elf);
    generate_shdr_info_tables(o, elf);
    w(o, 4, "</td>");

    w(o, 4, "<td class='infotables'>");
    generate_segment_info_tables(o, elf);
    generate_section_info_tables(o, elf);
    w(o, 4, "</td>");

    w(o, 3, "</tr>");
    w(o, 2, "</table>");
}

static void add_script(FILE *o, const char *const *src)
{
    w(o, 2, "<script type='text/javascript'>");
    indent_lines(o, src, 3);
    w(o, 2, "</script>");
}

static void add_scripts(FILE *o, const elf_t *elf)
{
    static const char *const classes[] = {
        "class", "data", "abi", "abi_ver", "e_type", "e_machine", "e_entry",
        "e_phoff", "e_shoff", "e_flags", "e_ehsize", "e_phentsize", "e_phnum",
        "e_shentsize", "e_shnum", "e_shstrndx",
    };

    /* highlight */
    w(o, 2, "<script type='text/javascript'>");
    indent_lines(o, HIGHLIGHT_JS, 3);
    for (size_t i = 0; i < sizeof classes / sizeof classes[0]; i++)
        w(o, 3, "highlightClasses('fileinfo_%s', '%s')", classes[i], classes[i]);
    w(o, 2, "</script>");

    add_script(o, DESCRIPTION_JS);
    add_script(o, CONCEAL_JS);

    /* offsets */
    w(o, 2, "<script type='text/javascript'>");
    w(o, 3, "let fileLen = %zu", elf->file_size);
    indent_lines(o, OFFSETS_JS, 3);
    w(o, 2, "</script>");

    /* arrows */
    w(o, 2, "<script type='text/javascript'>");
    indent_lines(o, ARROWS_JS, 3);
    w(o, 3, "connect('.e_phoff', '.bin_phdr0');");
    w(o, 3, "connect('.e_shoff', '.bin_shdr0');");
    for (size_t i = 0; i < elf->nphdrs; i++)
        w(o, 3, "connect('.bin_phdr%zu > .p_offset', '.bin_segment%zu');", i, i);
    for (size_t i = 0; i < elf->nshdrs; i++) {
        w(o, 3, "connect('.bin_shdr%zu > .sh_offset', '.bin_section%zu');", i, i);
        if (elf->shdrs[i].link != 0)
            w(o, 3, "connect('.bin_shdr%zu > .sh_link', '.bin_shdr%" PRIu64 "');",
              i, elf->shdrs[i].link);
    }
    w(o, 3, "pushArrowElems();");
    w(o, 2, "</script>");

    add_script(o, SETTINGS_JS);
}

static void put_span_open(FILE *o, const range_node *n)
{
    switch ((range_kind)n->kind) {
    case R_IDENT: fputs("<span class='ident'>", o); break;
    case R_FILE_HEADER: fputs("<span class='ehdr'>", o); break;
    case R_PROGRAM_HEADER:
        fprintf(o, "<span class='bin_phdr%" PRIu32 " phdr'>", n->idx); break;
    case R_SECTION_HEADER:
        fprintf(o, "<span class='bin_shdr%" PRIu32 " shdr'>", n->idx); break;
    case R_SEGMENT:
        fprintf(o, "<span class='bin_segment%" PRIu32 " segment'>", n->idx); break;
    case R_SECTION:
        fprintf(o, "<span class='bin_section%" PRIu32 " section hover'>", n->idx); break;
    case R_SEGMENT_SUBRANGE: fputs("<span class='segment_subrange hover'>", o); break;
    case R_HEADER_FIELD:
    case R_PHDR_FIELD:
    case R_SHDR_FIELD:
        fprintf(o, "<span class='%s'>", n->field); break;
    }
}

static void generate_file_dump(FILE *o, const elf_t *elf)
{
    const ranges_t *r = &elf->ranges;

    for (size_t idx = 0; idx < elf->file_size; idx++) {
        uint8_t byte = elf->contents[idx];

        for (int32_t n = r->head[idx]; n >= 0; n = r->nodes[n].next)
            put_span_open(o, &r->nodes[n]);

        if (idx < 4) {
            if (is_ascii_graphic(byte))
                fprintf(o, "&nbsp;%c", byte);
            else
                put_hex_byte(o, byte);
        } else {
            put_hex_byte(o, byte);
        }

        for (uint32_t k = 0; k < r->ends[idx]; k++)
            fputs("</span>", o);

        if (idx != elf->file_size - 1)
            fputc((idx + 1) % DEFAULT_COLUMNS == 0 ? '\n' : ' ', o);
    }
}

static void generate_ascii_dump(FILE *o, const elf_t *elf)
{
    for (size_t i = 0; i < elf->file_size; i++) {
        uint8_t b = elf->contents[i];

        if (is_ascii_graphic(b)) {
            const char *esc = html_escape(b);
            if (esc)
                fputs(esc, o);
            else
                fputc(b, o);
        } else {
            fputc('.', o);
        }

        if ((i + 1) % DEFAULT_COLUMNS == 0)
            fputc('\n', o);
    }
}

static void generate_body(FILE *o, const elf_t *elf)
{
    size_t len = elf->file_size;

    w(o, 1, "<body>");

    generate_svg_element(o);

    w(o, 2, "<div id='rightmenu'>");
    generate_right_menu(o);
    w(o, 2, "</div>");

    generate_file_info_table(o, elf);

    wnonl(o, 2, "<div id='offsets'>");
    for (size_t off = 0; off < len; off += DEFAULT_COLUMNS) {
        fprintf(o, "%zx", off);
        if (off != (len / DEFAULT_COLUMNS) * DEFAULT_COLUMNS)
            fputc('\n', o);
    }
    w(o, 0, "</div>");

    wnonl(o, 2, "<div id='bytes'>");
    generate_file_dump(o, elf);
    w(o, 0, "</div>");

    wnonl(o, 2, "<div id='ascii'>");
    generate_ascii_dump(o, elf);
    w(o, 0, "</div>");

    generate_sticky_info_tables(o, elf);

    add_scripts(o, elf);

    w(o, 1, "</body>");
}

static void generate_report(FILE *o, const elf_t *elf, const char *title)
{
    w(o, 0, "<!doctype html>");
    w(o, 0, "<html>");
    generate_head(o, elf, title);
    generate_body(o, elf);
    w(o, 0, "</html>");
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

static void usage(int ret)
{
    printf("Usage: elfcat <filename>\n");
    printf("Writes <filename>.html to CWD.\n");
    exit(ret);
}

static const char *parse_arguments(int argc, char **argv)
{
    if (argc != 2)
        usage(1);

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
        usage(0);

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("elfcat %s\n", ELFCAT_VERSION);
        exit(0);
    }

    return argv[1];
}

static uint8_t *read_file(const char *path, size_t *out_len)
{
    FILE *f;
    uint8_t *buf = NULL;
    size_t len = 0, cap = 0;

    errno = 0;
    f = fopen(path, "rb");
    if (!f)
        die("read file \"%s\": %s (os error %d)", path, strerror(errno), errno);

    for (;;) {
        size_t n;
        if (len == cap) {
            cap = cap ? cap * 2 : 65536;
            buf = xrealloc(buf, cap);
        }
        n = fread(buf + len, 1, cap - len, f);
        len += n;
        if (n == 0) {
            if (ferror(f)) {
                int e = errno;
                die("read file \"%s\": %s (os error %d)", path, strerror(e), e);
            }
            break;
        }
    }

    fclose(f);
    *out_len = len;
    return buf;
}

int main(int argc, char **argv)
{
    const char *filename = parse_arguments(argc, argv);
    size_t len;
    uint8_t *contents = read_file(filename, &len);
    elf_t elf;
    char *err = NULL;
    char *base;
    char *report_filename;
    FILE *o;

    if (!parse_elf(&elf, filename, contents, len, &err))
        die("parse ELF: %s", err);

    base = basename_dup(filename);
    if (!base)
        die("construct filename");
    report_filename = xasprintf("%s.html", base);

    errno = 0;
    o = fopen(report_filename, "wb");
    if (!o)
        die("write report: %s (os error %d)", strerror(errno), errno);
    setvbuf(o, NULL, _IOFBF, 1 << 20);

    generate_report(o, &elf, base);

    if (ferror(o) | (fclose(o) != 0))
        die("write report: %s (os error %d)", strerror(errno), errno);

    return 0;
}