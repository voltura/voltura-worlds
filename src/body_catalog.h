#pragma once
#include "model.h"

namespace worlds {
struct BodyFacts {
 Scene scene;
 const wchar_t *name,*classification,*diameter,*earthDistance,*atmosphere,*characteristic1,*characteristic2;
 std::array<unsigned char,3> accent;
};
// Mean diameters and deliberately rounded orbital distance envelopes. Sources
// and the distance convention are documented in docs/body-statistics.md.
inline constexpr std::array bodyCatalog{
 BodyFacts{Scene::Mercury,L"Mercury",L"Terrestrial planet",L"4,879 km",L"Approx. 77–222 million km",L"Extremely thin exosphere",L"Smallest planet in the Solar System.",L"A heavily cratered, rocky surface.",{202,183,160}},
 BodyFacts{Scene::Venus,L"Venus",L"Terrestrial planet",L"12,104 km",L"Approx. 38–261 million km",L"Dense carbon dioxide; acid clouds",L"The hottest planet in our Solar System.",L"Rotates in the opposite direction to Earth.",{238,202,138}},
 BodyFacts{Scene::Earth,L"Earth",L"Terrestrial planet",L"12,742 km",L"Your home planet",L"78% nitrogen, 21% oxygen",L"Liquid oceans cover about 71% of its surface.",L"The only world known to support life.",{112,188,230}},
 BodyFacts{Scene::Moon,L"Moon",L"Earth's natural satellite",L"3,475 km",L"Mean: 384,400 km",L"Extremely thin exosphere",L"Keeps the same face turned toward Earth.",L"Dark plains record ancient volcanic activity.",{193,204,220}},
 BodyFacts{Scene::Mars,L"Mars",L"Terrestrial planet",L"6,779 km",L"Approx. 55–401 million km",L"Thin, mostly carbon dioxide",L"Iron minerals give its surface a rusty hue.",L"Home to the giant volcano Olympus Mons.",{233,156,118}},
 BodyFacts{Scene::Jupiter,L"Jupiter",L"Gas giant",L"139,822 km",L"Approx. 0.59–0.97 billion km",L"Mostly hydrogen and helium",L"The largest planet in our Solar System.",L"The Great Red Spot is a vast, lasting storm.",{226,186,151}},
 BodyFacts{Scene::Saturn,L"Saturn",L"Gas giant",L"116,464 km",L"Approx. 1.2–1.7 billion km",L"Mostly hydrogen and helium",L"Its bright rings contain countless icy pieces.",L"Its average density is lower than water's.",{227,204,158}},
 BodyFacts{Scene::Uranus,L"Uranus",L"Ice giant",L"50,724 km",L"Approx. 2.6–3.2 billion km",L"Hydrogen, helium and methane",L"Rotates on its side, tilted about 98 degrees.",L"Methane absorbs red light, giving a blue tint.",{149,216,219}},
 BodyFacts{Scene::Neptune,L"Neptune",L"Ice giant",L"49,244 km",L"Approx. 4.3–4.7 billion km",L"Hydrogen, helium and methane",L"The most distant planet from the Sun.",L"Powerful winds race through its atmosphere.",{130,165,231}},
 BodyFacts{Scene::Pluto,L"Pluto",L"Dwarf planet",L"2,376 km",L"Approx. 4.3–7.5 billion km",L"Thin nitrogen with methane and CO",L"An icy world in the distant Kuiper Belt.",L"A bright heart-shaped region spans its surface.",{205,183,166}},
 BodyFacts{Scene::Sun,L"Sun",L"Star · yellow dwarf",L"Approx. 1.4 million km",L"Mean: 149.6 million km",L"Hot plasma; an extended corona",L"Hydrogen fusion in its core releases energy.",L"Its gravity holds the Solar System together.",{255,165,98}}
};
inline const BodyFacts* bodyFacts(Scene scene){for(const auto& body:bodyCatalog)if(body.scene==scene)return &body;return nullptr;}
}
