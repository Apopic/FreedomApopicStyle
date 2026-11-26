#pragma once
#include "../Library/AudioAlignment.hpp"
static void AudioAlignmentTests() {
    const auto allow=[](size_t,int64_t,int64_t){return true;};
    const auto clock=[](double p){return p*2000;};
    const auto quarters=audioalign::FitGrid({.2504,.4996,.7503},1,clock,allow);
    Check(quarters && quarters->Division==4 && quarters->Slots==std::vector<int64_t>({1,2,3}),"wave alignment prefers coarse shared grid within 10ms");
    Check(audioalign::FitGrid({.254},1,clock,allow)->Division==4,"default grid tolerance accepts 8ms error");
    const auto thirds=audioalign::FitGrid({1.0/3+.0002,2.0/3-.0002},4,clock,allow);
    Check(thirds && thirds->Division==12,"wave grid accounts for fixed note and command denominators");
    const auto slow=audioalign::FitGrid({.2514},1,[](double p){return p*10000;},allow);
    Check(slow && slow->Division>4,"grid error is measured in time at slow BPM");
    const auto exact=audioalign::FitGrid({.123456,.56789},1,clock,allow);
    Check(exact && std::abs(clock(double(exact->Slots[0])/exact->Division)-clock(.123456))<=10.000001,"irregular timing keeps waveform accuracy");
    Check(!audioalign::FitGrid({.5},1,clock,[](size_t,int64_t,int64_t){return false;}),"grid respects selection and stationary-note constraints");
    Check(!audioalign::FitGrid({.5,.5},1,clock,allow),"grid cannot merge or reorder notes");
    Check(!audioalign::FitGrid({std::numeric_limits<double>::quiet_NaN()},1,clock,allow),"invalid grid position rejected");
    const int rate = 16000;
    std::vector<float> samples(rate * 3, 0);
    for (double t : { 0.43, 1.12, 1.83, 2.48 }) {
        for (int i = 0; i < 480; ++i) samples[(size_t)(t * rate) + i] += (float)(0.8 * std::sin(i * 2 * 3.14159265 * 100 / rate) * std::exp(-(double)i / rate * 180));
    }
    const auto onsets = audioalign::Detect(samples.data(), samples.size(), rate);
    const auto snapped = audioalign::Snap({0.5,1,2,2.5},{true,true,true,false},onsets,0.25);
    Check(std::abs(snapped[0]-.43)<.004 && std::abs(snapped[1]-1.12)<.004 && std::abs(snapped[2]-1.83)<.004, "rough notes align to detected attacks");
    Check(snapped[3]==2.5 && std::is_sorted(snapped.begin(),snapped.end()), "fixed notes and order preserved");
    Check(audioalign::Snap({0.5},{true},onsets,.02)[0]==.5, "outside tolerance unchanged");
    Check(audioalign::Snap({0.5},{true},{},.25)[0]==.5, "missing attacks unchanged");
    const auto ambiguous=audioalign::Snap({.9,1.1},{true,true},{{1,1}},.25);
    Check(ambiguous[0]<ambiguous[1], "one attack does not merge two notes");
    std::fill(samples.begin(),samples.end(),0.0f);
    Check(audioalign::Detect(samples.data(),samples.size(),rate).empty(), "silence has no alignment targets");
    for(int i=0;i<640;++i) samples[rate+i]=(float)(i<=240?i/240.0:(640-i)/400.0);
    const auto ramp=audioalign::Detect(samples.data(),samples.size(),rate);
    Check(!ramp.empty() && std::abs(ramp.front().Time-1.015)<.001,"waveform apex selected instead of attack threshold");
}
