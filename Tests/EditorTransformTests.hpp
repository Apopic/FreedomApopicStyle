#pragma once
#include "../Library/UniformScroll.hpp"
static void EditorTransformTests() {
    auto doc=tja::Document::NewChart("uniform scroll","",3);
    auto& c=doc.Courses[0];
    tja::Document::SetNote(c.Measures[0],1,4,'1');
    tja::Document::AddCommand(c.Measures[0],0,1,"#SCROLL 1.5");
    tja::Document::AddCommand(c.Measures[0],1,2,"#BPMCHANGE 240");
    tja::Document::AddCommand(c.Measures[0],3,4,"#SCROLL 3");
    tja::Document::AddCommand(c.Measures[1],0,1,"#BPMCHANGE 60");
    tja::Document::AddCommand(c.Measures[1],1,4,"#BPMCHANGE 90");
    tja::Document::AddCommand(c.Measures[1],1,4,"#BPMCHANGE 180 // final value");
    tja::Document::AddCommand(c.Measures[2],0,1,"#SCROLL -2");
    const auto before=doc.Write();
    auto result=uniformscroll::Apply(c,"120",{0,1,4},{1,1,2},120);
    auto at=[&](int m,int64_t n,int64_t d) {
        std::vector<std::string> out;
        for(const auto& cmd:result.Measures[m].Commands)
            if(tja::ComparePos(cmd.Num,cmd.Den,n,d)==0 && uniformscroll::Is(cmd.Text,"#SCROLL"))out.push_back(cmd.Text);
        return out;
    };
    Check(at(0,0,1)==std::vector<std::string>{"#SCROLL 1.5"},"scroll before selection preserved");
    Check(at(0,1,4)==std::vector<std::string>{"#SCROLL 1"} && at(0,1,2)==std::vector<std::string>{"#SCROLL 0.5"},"visual BPM compensates initial and mid-measure BPM");
    Check(at(0,3,4).empty() && at(1,0,1)==std::vector<std::string>{"#SCROLL 2"},"conflicting scrolls removed and measure boundary BPM compensated");
    Check(at(1,1,4).size()==1 && std::abs(std::stod(at(1,1,4)[0].substr(8))*180-120)<1e-10,"same-position BPM changes use final value");
    Check(at(1,1,2)==std::vector<std::string>{"#SCROLL 3"} && at(2,0,1)==std::vector<std::string>{"#SCROLL -2"},"scroll restored at end and following commands untouched");
    Check(result.Measures[0].Notes[0].Num==1 && result.Measures[0].Notes[0].Den==4 && doc.Write()==before,"notes and source document unchanged");
    auto applied=doc;applied.Courses[0]=result;
    std::string serialized=applied.Write();std::vector<std::string_view> lines;
    for(size_t begin=0;begin<serialized.size();) {auto end=serialized.find('\n',begin);if(end==std::string::npos)end=serialized.size();lines.emplace_back(serialized.data()+begin,end-begin);begin=end+1;}
    Check(tja::Document::Parse(lines).Write()==serialized,"uniform-scroll commands survive save and reload");
    tja::Document::AddCommand(c.Measures[1],1,2,"#SCROLL 7");
    result=uniformscroll::Apply(c,"120",{0,1,2},{1,1,2},240);
    Check(at(0,1,2)==std::vector<std::string>{"#SCROLL 1"} && at(1,1,2)==std::vector<std::string>{"#SCROLL 7"},"start BPM at selection and existing end command respected");
    bool rejected=false;
    try {uniformscroll::Apply(c,"120",{0,0,1},{1,1,1},0);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"nonpositive visual BPM rejected");
    tja::Document::AddCommand(c.Measures[0],0,1,"#BRANCHSTART p,80,90");
    rejected=false;
    try {uniformscroll::Apply(c,"120",{0,0,1},{1,1,1},120);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"unsupported branch transform rejected before modifying document");
}
