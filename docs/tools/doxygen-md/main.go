package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"path/filepath"
	"runtime"
	"sort"
	"strings"

	"github.com/tidwall/gjson"
	"github.com/tidwall/pretty"
	"github.com/tidwall/x2j"
)

const ShowIndexBullets = true

func convertXMLDirectoryToIndexJSON() error {
	entries, err := os.ReadDir("xml")
	if err != nil {
		return err
	}
	var dst []byte
	dst = append(dst, '{')
	var n int
	for _, entry := range entries {
		if !strings.HasSuffix(entry.Name(), ".xml") {
			continue
		}
		xmldata, err := os.ReadFile(filepath.Join("xml", entry.Name()))
		if err != nil {
			return err
		}
		jraw, err := x2j.Convert(xmldata)
		if err != nil {
			return err
		}
		name := entry.Name()[:len(entry.Name())-4]
		if n > 0 {
			dst = append(dst, ',')
		}
		dst = gjson.AppendJSONString(dst, name)
		dst = append(dst, ':')
		dst = append(dst, jraw...)
		n++
	}
	dst = append(dst, '}')
	return os.WriteFile("index.json", pretty.Pretty(dst), 0666)
}

func modDedup(json string, arg string) string {
	arr := gjson.Parse(json)
	if !arr.IsArray() {
		return ""
	}
	all := make(map[string]bool)
	var out []byte
	out = append(out, '[')
	arr.ForEach(func(_, res gjson.Result) bool {
		str := res.String()
		if exists := all[str]; !exists {
			if len(all) > 0 {
				out = append(out, ',')
			}
			out = append(out, res.Raw...)
			all[str] = true
		}
		return true
	})
	out = append(out, ']')
	return string(out)
}

func genDefsJSON(ns string, json string) gjson.Result {
	// Get array of all valid files. Includes *.c, *.h, etc.
	files := gjson.Parse(json).
		Get(`index.children.#(name=compound)#`).
		Get(`#(attrs.kind=file)#`)

	// Get the first known public public header file
	hfile := files.
		Get(`#(children.0.name=name)#`).
		Get(`#(children.0.children.0%*.h)#`).
		Get(`0`)
	hid := hfile.Get("attrs.refid").String()

	// header details file
	hdetails := gjson.Get(json, hid)

	// Get innerclass types, which are struct that are declared in the header
	// which members that should be shown (not private).
	hpublictypes := hdetails.
		Get("children.#(name=compounddef)#").
		Get("#.children.#(name=innerclass)#").
		Get("@flatten")

	// Get a list of all refids
	refids0 := hfile.Get(`@dig:refid`)
	refids1 := hdetails.Get(`@dig:refid`)
	refids2 := gjson.Parse(json).
		Get(`index.children.#(name=compound)#`).
		Get(`#(attrs.kind=group)#`).
		Get(`#.attrs.refid`)
	refidsAll := "[" + refids0.Raw + "," + refids1.Raw + "," + refids2.Raw + "]"
	refids := gjson.Get(refidsAll, "@flatten|@dedup")

	mdefs := gjson.Get(json, `@dig:#(name=memberdef)#|@flatten`)
	nprocs := runtime.NumCPU()
	chout := make(chan string)
	chin := make(chan string)
	chdoutone := make(chan bool)
	for i := 0; i < nprocs; i++ {
		go func(i int) {
			for refid := range chin {
				hdef := mdefs.Get(`#(attrs.id=` + refid + `)`)
				cdef := gjson.Get(json, refid).
					Get("children.#(name=compounddef)")
				name := ""
				kind := ""
				if hdef.Exists() {
					name = hdef.
						Get("children.#(name=name)|children.0").String()
					kind = hdef.Get("attrs.kind").String()
				} else if cdef.Exists() {
					name = cdef.
						Get("children.#(name=compoundname)|children.0").String()
					kind = cdef.Get("attrs.kind").String()
				}
				if name == "" || kind == "file" {
					continue
				}
				if kind != "group" && !strings.HasPrefix(name, ns) {
					continue
				}
				showdef := hpublictypes.Get("#(children.0="+name+")").
					Get("attrs.prot").String() == "public"
				var out []byte
				out = append(out, '{')
				out = append(out, `"name":`...)
				out = gjson.AppendJSONString(out, name)
				out = append(out, `,"kind":`...)
				out = gjson.AppendJSONString(out, kind)
				out = append(out, `,"refid":`...)
				out = gjson.AppendJSONString(out, refid)
				if hdef.Exists() {
					out = append(out, `,"hdef":`...)
					out = append(out, hdef.Raw...)
				}
				if cdef.Exists() {
					out = append(out, `,"cdef":`...)
					out = append(out, cdef.Raw...)
				}
				if showdef {
					out = append(out, `,"showdef":true`...)
				}
				out = append(out, '}')
				chout <- string(out)
			}
			chdoutone <- true
		}(i)
	}
	all := make(map[string]string)
	go func() {
		for out := range chout {
			all[gjson.Get(out, "refid").String()] = out
		}
		chdoutone <- true
	}()
	refids.ForEach(func(_, res gjson.Result) bool {
		chin <- res.String()
		return true
	})
	close(chin)
	for i := 0; i < nprocs; i++ {
		<-chdoutone
	}
	close(chout)
	<-chdoutone
	var out []byte
	out = append(out, '[')
	n := 0
	refids.ForEach(func(_, res gjson.Result) bool {
		refid := res.String()
		if val, ok := all[refid]; ok {
			if n > 0 {
				out = append(out, ',')
			}
			out = append(out, val...)
			n++
		}
		return true
	})
	out = append(out, ']')
	defs := string(out) // defs, unordered
	arr := gjson.Parse(defs).Array()
	sort.SliceStable(arr, func(i, j int) bool {
		a := arr[i].Get("hdef.children.#(name=location).attrs.line")
		if !a.Exists() {
			a = arr[i].Get("cdef.children.#(name=location).attrs.line")
		}
		b := arr[j].Get("hdef.children.#(name=location).attrs.line")
		if !b.Exists() {
			b = arr[j].Get("cdef.children.#(name=location).attrs.line")
		}
		return a.Int() < b.Int()
	})

	out = append(out[:0], '[')
	for i := range arr {
		if i > 0 {
			out = append(out, ',')
		}
		out = append(out, arr[i].Raw...)
	}
	out = append(out, ']')
	defs = string(out) // defs, ordered
	return gjson.Parse(defs).Get("@pretty")
}

func main() {
	gjson.AddModifier("dedup", modDedup)

	var ns string
	flag.StringVar(&ns, "ns", "", "public namespace")
	flag.Parse()

	if err := convertXMLDirectoryToIndexJSON(); err != nil {
		log.Fatal(err)
	}
	jsondata, err := os.ReadFile("index.json")
	if err != nil {
		log.Fatal(err)

	}
	defs := genDefsJSON(ns, string(jsondata))
	os.WriteFile("defs.json", []byte(defs.Raw), 0666)

	dox := Doxygen{defs}
	md := dox.Markdown()
	os.Stdout.Write([]byte(md))

}

const briefDescPath = "children.#(name=briefdescription).children." +
	"#(name=para).children"

func toSimpleText(text gjson.Result) string {
	var str string
	text.ForEach(func(_, part gjson.Result) bool {
		if part.Type == gjson.String {
			str += part.String()
		} else {
			str += toSimpleText(part.Get("children"))
		}
		return true
	})
	return str
}

type Doxygen struct{ gjson.Result }
type Struct struct{ gjson.Result }
type StructMember struct{ gjson.Result }
type Enum struct{ gjson.Result }
type EnumMember struct{ gjson.Result }
type Func struct{ gjson.Result }
type FuncParam struct{ gjson.Result }
type Group struct{ gjson.Result }

func mdwrap(list gjson.Result, start, end string, plain bool) string {
	var md string
	if !plain {
		md += start
	}
	// if len(start) > 0 && start[0] == '`' {
	// 	md += markdownDescPara(list, true)
	// } else {
	md += markdownDesc(list, plain)
	// }
	if !plain {
		md += end
	}
	return md
}

func markdownDesc(list gjson.Result, plain bool) string {
	var md string
	var lastsimplekind string
	list.ForEach(func(_, val gjson.Result) bool {
		if val.IsObject() {
			switch val.Get("name").String() {
			case "para":
				md += markdownDesc(val.Get("children"), plain)
				md += "\n\n"
			case "parameterlist":
				md += "\n\n**Parameters**\n\n"
				params := val.Get("children.#(name=parameteritem)#|@pretty")
				params.ForEach(func(_, pval gjson.Result) bool {
					name := markdownDesc(
						pval.Get("children.#(name=parameternamelist)").
							Get("children.#(name=parametername)").
							Get("children"), plain)
					desc := markdownDesc(
						pval.Get("children.#(name=parameterdescription)").
							Get("children"), plain)
					name = strings.TrimSpace(name)
					desc = strings.TrimSpace(desc)
					md += "- **" + name + "**: " + desc + "\n"
					return true
				})
				md += "\n"
			case "simplesect":
				kind := val.Get("attrs.kind").String()
				if kind != "" {
					if kind == "see" {
						kind = "See also"
					}
					kind = strings.ToUpper(kind[0:1]) + kind[1:]
				}
				if lastsimplekind != kind {
					md += "\n\n**" + kind + "**\n\n"
				}
				lastsimplekind = kind
				line := markdownDesc(val.Get("children"), plain)
				line = strings.TrimSpace(line)
				md += "- " + line + "\n"
			case "ref":
				refid := val.Get("attrs.refid").String()
				text := val.Get("children.0").String()
				if !plain && refid != "" && text != "" {
					md += "[" + text + "](#" + refid + ")"
				} else {
					md += text
				}
			case "ulink":
				url := val.Get("attrs.url").String()
				text := val.Get("children.0").String()
				if !plain && url != "" && text != "" {
					md += "[" + text + "](" + url + ")"
				} else {
					md += text
				}
			case "computeroutput":
				md += mdwrap(val.Get("children"), "`", "`", plain)
			case "emphasis":
				md += mdwrap(val.Get("children"), "*", "*", plain)
			case "bold":
				md += mdwrap(val.Get("children"), "**", "**", plain)
			case "programlisting":
				md += mdwrap(val.Get("children"), "```c\n", "```\n", plain)
			case "codeline":
				md += markdownDesc(val.Get("children"), true) + "\n"
			case "sp":
				md += " "
			default:
				// println(val.Get("name").String())
				md += markdownDesc(val.Get("children"), plain)
			}
		} else {
			md += val.String()
		}
		return true
	})
	return md
}

func (dox *Doxygen) Structs() []Struct {
	var structs []Struct
	path := "#(kind=struct)#"
	dox.Get(path).ForEach(func(_, val gjson.Result) bool {
		if val.Get("cdef.children.#(name=includes)").Exists() {
			structs = append(structs, Struct{val})
		}
		return true
	})
	return structs
}

func (dox *Doxygen) Objects() []Struct {
	// objects are structs that do not have a header structure.
	var structs []Struct
	path := "#(kind=struct)#"
	dox.Get(path).ForEach(func(_, val gjson.Result) bool {
		if !val.Get("cdef.children.#(name=includes)").Exists() {
			structs = append(structs, Struct{val})
		}
		return true
	})
	return structs
}

func (dox *Doxygen) Typedefs() []Struct {
	var structs []Struct
	path := "#(kind=typedef)#"
	dox.Get(path).ForEach(func(_, val gjson.Result) bool {
		if !val.Get("cdef.children.#(name=includes)").Exists() {
			structs = append(structs, Struct{val})
		}
		return true
	})
	return structs
}

func getDetailsChildren(val gjson.Result) gjson.Result {
	arr := val.Get("hdef.children.#(name=detaileddescription).children")
	if !arr.Exists() {
		arr = val.Get("cdef.children.#(name=detaileddescription).children")
	}
	if !arr.Exists() {
		arr = val.Get("hdef.children.#(name=briefdescription).children")
	}
	if !arr.Exists() {
		arr = val.Get("cdef.children.#(name=briefdescription).children")
	}
	return arr
}

func getBriefChildren(val gjson.Result) gjson.Result {
	arr := val.Get("hdef.children.#(name=briefdescription).children")
	if !arr.Exists() {
		arr = val.Get("cdef.children.#(name=briefdescription).children")
	}
	return arr
}

func (val *Struct) DetailsMarkdown(plain bool) string {
	list := getDetailsChildren(val.Result)
	return markdownDesc(list, plain)
}

func (val *Struct) BriefMarkdown(plain bool) string {
	list := getBriefChildren(val.Result)
	md := strings.TrimSpace(markdownDesc(list, plain))
	return md
}

func (val *Struct) Name() string {
	return val.Get("name").String()
}

func (val *Struct) RefID() string {
	return val.Get("refid").String()
}

func (val *Struct) Signature() string {
	sig := "struct " + val.Name()
	if val.Get("showdef").Bool() {
		sig += " {\n"
		var maxnlen int
		for _, mdef := range val.Members() {
			label := mdef.Type() + " " + mdef.Name() + ";"
			if len(label) > maxnlen {
				maxnlen = len(label)
			}
		}
		for _, mdef := range val.Members() {
			label := mdef.Type() + " " + mdef.Name() + ";"
			if maxnlen > 0 {
				label += strings.Repeat(" ", maxnlen-len(label))
			}
			sig += "    " + label
			brief := mdef.Brief()
			if brief != "" {
				sig += " // " + brief
			}
			sig += "\n"
		}
		sig += "}"
	}
	sig += ";"
	return sig
}

func (val *Struct) Members() []StructMember {
	var members []StructMember
	path := "cdef.children.#(name=sectiondef).children.#(name=memberdef)#"
	val.Get(path).ForEach(func(_, mdef gjson.Result) bool {
		members = append(members, StructMember{mdef})
		return true
	})
	return members
}

func (val *StructMember) Type() string {
	path := "children.#(name=type).children"
	return toSimpleText(val.Get(path))
}

func (val *StructMember) Name() string {
	return val.Get("children.#(name=name).children.0").String()
}

func (val *StructMember) Brief() string {
	return toSimpleText(val.Get(briefDescPath))
}

func (val *Enum) Name() string {
	return val.Get("name").String()
}

func (val *Enum) RefID() string {
	return val.Get("refid").String()
}

func (val *Enum) DetailsMarkdown(plain bool) string {
	list := getDetailsChildren(val.Result)
	return markdownDesc(list, plain)
}

func (val *Enum) BriefMarkdown(plain bool) string {
	list := getBriefChildren(val.Result)
	md := strings.TrimSpace(markdownDesc(list, plain))
	return md
}

func (val *Enum) Members() []EnumMember {
	var members []EnumMember
	path := "hdef.children.#(name=enumvalue)#"
	val.Get(path).ForEach(func(_, mdef gjson.Result) bool {
		members = append(members, EnumMember{mdef})
		return true
	})
	return members
}
func (val *Enum) Signature() string {
	sig := "enum " + val.Name() + " {\n"
	var maxnlen int
	var maxilen int
	for _, eval := range val.Members() {
		nlen := len(eval.Name())
		if nlen > maxnlen {
			maxnlen = nlen
		}
		ilen := len(eval.Initializer())
		if ilen > maxilen {
			maxilen = ilen
		}
	}
	for _, eval := range val.Members() {
		name := eval.Name()
		initializer := eval.Initializer()
		brief := eval.Brief()
		label := name
		if maxilen > 0 {
			label += strings.Repeat(" ", maxnlen-len(name))
			label += " "
			label += initializer
		}
		label += ","
		if maxilen == 0 {
			label += strings.Repeat(" ", maxnlen-len(name))
		} else {
			label += strings.Repeat(" ", maxilen-len(initializer))
		}
		if brief != "" {
			label += " // " + brief
		}
		label = strings.TrimSpace(label)
		sig += "    " + label + "\n"
	}
	sig += "};"
	return sig
}

func (val *EnumMember) Name() string {
	return val.Get("children.#(name=name).children.0").String()
}

func (val *EnumMember) Initializer() string {
	return val.Get("children.#(name=initializer).children.0").String()
}
func (val *EnumMember) Brief() string {
	return toSimpleText(val.Get(briefDescPath))
}

func (dox *Doxygen) Enums() []Enum {
	var enums []Enum
	path := "#(kind=enum)#"
	dox.Get(path).ForEach(func(_, val gjson.Result) bool {
		enums = append(enums, Enum{val})
		return true
	})
	return enums
}

func (dox *Doxygen) Funcs() []Func {
	var vals []Func
	path := "#(kind=function)#"
	dox.Get(path).ForEach(func(_, val gjson.Result) bool {
		vals = append(vals, Func{val})
		return true
	})
	return vals
}

func (val *Func) Name() string {
	return val.Get("name").String()
}

func (val *Func) RefID() string {
	return val.Get("refid").String()
}

func (val *Func) Signature() string {
	typ := toSimpleText(val.Get("hdef.children.#(name=type).children"))
	name := val.Get("hdef.children.#(name=name).children.0").String()
	args := val.Get("hdef.children.#(name=argsstring).children.0").String()
	sig := typ
	if !strings.HasSuffix(sig, "*") {
		sig += " "
	}
	args = strings.TrimSpace(args)
	if args == "(void)" {
		args = "()"
	}
	sig += strings.TrimSpace(name) + args + ";"
	return sig
}

func (val *Func) DetailsMarkdown(plain bool) string {
	list := getDetailsChildren(val.Result)
	return markdownDesc(list, plain)
}

func (val *Func) BriefMarkdown(plain bool) string {
	list := getBriefChildren(val.Result)
	return markdownDesc(list, plain)
}

func (val *FuncParam) Name() string {
	path := "children.#(name=parameternamelist)." +
		"children.#(name=parametername).children.0"
	return val.Get(path).String()
}

func (dox *Doxygen) Groups() []Group {
	var vals []Group
	path := "#(kind=group)#"
	dox.Get(path).ForEach(func(_, val gjson.Result) bool {
		vals = append(vals, Group{val})
		return true
	})
	return vals
}

func (val *Group) Name() string {
	return val.Get("name").String()
}

func (val *Group) RefID() string {
	return val.Get("refid").String()
}

func (val *Group) DetailsMarkdown(plain bool) string {
	list := getDetailsChildren(val.Result)
	return markdownDesc(list, plain)
}

func (val *Group) BriefMarkdown(plain bool) string {
	list := getBriefChildren(val.Result)
	return markdownDesc(list, plain)
}

// func (val *Group) Desc() string {
// 	const path = "cdef.children.#(name=detaileddescription).children." +
// 		"#(name=para).children"
// 	return toSimpleText(val.Get(path))
// }

func (val *Group) Title() string {
	path := "cdef.children.#(name=title).children"
	return toSimpleText(val.Get(path))
}

func (dox *Doxygen) writeIndexMarkdown(wr io.Writer) {
	const idxfmt = "[%s](#%s)"

	// fmt.Fprintf(wr, "## Structs\n\n")
	var n int
	// for _, val := range dox.Structs() {
	// 	if ShowIndexBullets {
	// 		fmt.Fprintf(wr, "- "+idxfmt+"\n", val.Name(), val.RefID())
	// 		// fmt.Fprintf(wr, idxfmt+"  \n", val.Name(), val.RefID())
	// 	} else {
	// 		if n > 0 {
	// 			fmt.Fprintf(wr, ",\n")
	// 		}
	// 		fmt.Fprintf(wr, idxfmt, val.Name(), val.RefID())
	// 		n++
	// 	}
	// }

	// tdefs := dox.Typedefs()
	n = 0
	// if len(tdefs) > 0 {
	// 	fmt.Fprintf(wr, "## Types\n\n")
	// 	for _, val := range dox.Typedefs() {
	// 		if ShowIndexBullets {
	// 			fmt.Fprintf(wr, "- "+idxfmt+"\n", val.Name(), val.RefID())
	// 			// fmt.Fprintf(wr, idxfmt+"  \n", val.Name(), val.RefID())
	// 		} else {
	// 			if n > 0 {
	// 				fmt.Fprintf(wr, ",\n")
	// 			}
	// 			fmt.Fprintf(wr, idxfmt, val.Name(), val.RefID())
	// 			n++
	// 		}
	// 	}
	// }
	// fmt.Fprintf(wr, "## Objects\n\n")
	// n = 0;
	// for _, val := range dox.Objects() {
	// 	if ShowIndexBullets {
	// 		fmt.Fprintf(wr, "- "+idxfmt+"\n", val.Name(), val.RefID())
	// 		// fmt.Fprintf(wr, idxfmt+"  \n", val.Name(), val.RefID())
	// 	} else {
	// 		if n > 0 {
	// 			fmt.Fprintf(wr, ",\n")
	// 		}
	// 		fmt.Fprintf(wr, idxfmt, val.Name(), val.RefID())
	// 		n++
	// 	}
	// }

	enums := dox.Enums()
	if len(enums) > 0 {
		fmt.Fprintf(wr, "## Enums\n\n")
		n = 0
		for _, val := range enums {
			if ShowIndexBullets {
				fmt.Fprintf(wr, "- "+idxfmt+"\n", val.Name(), val.RefID())
				// fmt.Fprintf(wr, idxfmt+"  \n", val.Name(), val.RefID())
			} else {
				if n > 0 {
					fmt.Fprintf(wr, ",\n")
				}
				fmt.Fprintf(wr, idxfmt, val.Name(), val.RefID())
				n++
			}
		}
	}
	fmt.Fprintf(wr, "\n\n")

	groups := dox.Groups()

	var lastGroupID string
	n = 0
	for _, val := range dox.Funcs() {
		var group Group
		var ngroupf string
		if strings.HasPrefix(val.RefID(), "group__") {
			for j := range groups {
				if strings.HasPrefix(val.RefID(), groups[j].RefID()) {
					if len(groups[j].RefID()) > len(ngroupf) {
						ngroupf = groups[j].RefID()
						group = groups[j]
					}
				}
			}
		}
		if group.RefID() != lastGroupID {
			if n > 0 {
				fmt.Fprintf(wr, "\n\n")
			}
			fmt.Fprintf(wr, "<a name='%s'></a>\n", group.RefID())
			fmt.Fprintf(wr, "## %s\n\n", group.Title())
			desc := group.DetailsMarkdown(false)
			if desc != "" {
				fmt.Fprintf(wr, "%s\n\n", desc)
			}
			lastGroupID = group.RefID()
			n = 0
		}
		if ShowIndexBullets {
			fmt.Fprintf(wr, "- "+idxfmt+"\n", val.Name()+"()", val.RefID())
			// fmt.Fprintf(wr, idxfmt+"  \n", val.Name()+"()", val.RefID())
		} else {

			if n > 0 {
				fmt.Fprintf(wr, ",\n")
			}
			fmt.Fprintf(wr, idxfmt, val.Name()+"()", val.RefID())
		}
		n++
	}
	fmt.Fprintf(wr, "\n")
}

type DefType interface {
	RefID() string
	Name() string
	Signature() string
	DetailsMarkdown(plain bool) string
}

func writeDefType(val DefType, wr io.Writer, plain bool, isfunc bool) {
	fmt.Fprintf(wr, "<a name='%s'></a>\n", val.RefID())
	var ex string
	if isfunc {
		ex = "()"
	}
	fmt.Fprintf(wr, "## %s%s\n", val.Name(), ex)
	fmt.Fprintf(wr, "```c\n")
	fmt.Fprintf(wr, "%s\n", val.Signature())
	fmt.Fprintf(wr, "```\n")
	fmt.Fprintf(wr, "%s\n", val.DetailsMarkdown(false))
}

func (dox *Doxygen) writeDefinitionsMarkdown(wr io.Writer) {
	for _, val := range dox.Structs() {
		writeDefType(&val, wr, false, false)
	}

	for _, val := range dox.Objects() {
		writeDefType(&val, wr, false, false)
	}

	for _, val := range dox.Enums() {
		writeDefType(&val, wr, false, false)
	}

	for _, val := range dox.Funcs() {
		writeDefType(&val, wr, false, true)
	}
}

func (dox *Doxygen) Markdown() string {
	out := strings.Builder{}
	dox.writeIndexMarkdown(&out)
	dox.writeDefinitionsMarkdown(&out)
	return out.String()
}
