{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 4,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 34.0, 94.0, 1260.0, 989.0 ],
        "boxes": [
            {
                "box": {
                    "id": "obj-14",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 161.0, 1090.0, 115.0, 22.0 ],
                    "saved_object_attributes": {
                        "versionnumber": 80300
                    },
                    "text": "bach.pick 3 @out m"
                }
            },
            {
                "box": {
                    "id": "obj-45",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 87.0, 149.0, 24.0, 24.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-43",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 116.0, 151.0, 431.0, 20.0 ],
                    "text": "<= to change dynamically the json file, edit the text below and then send a bang"
                }
            },
            {
                "box": {
                    "id": "obj-39",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 74.0, 15.0, 162.0, 20.0 ],
                    "text": "key V triggers the calculation"
                }
            },
            {
                "box": {
                    "id": "obj-35",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 70.0, 1356.0, 50.0, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-25",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "patching_rect": [ 170.0, 1010.0, 58.0, 22.0 ],
                    "text": "loadbang"
                }
            },
            {
                "box": {
                    "fontname": "Arial",
                    "fontsize": 13.0,
                    "id": "obj-26",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 170.0, 1047.0, 90.0, 23.0 ],
                    "text": "openWindow"
                }
            },
            {
                "box": {
                    "id": "obj-31",
                    "maxclass": "newobj",
                    "numinlets": 3,
                    "numoutlets": 0,
                    "patcher": {
                        "fileversion": 1,
                        "appversion": {
                            "major": 9,
                            "minor": 1,
                            "revision": 4,
                            "architecture": "x64",
                            "modernui": 1
                        },
                        "classnamespace": "box",
                        "rect": [ 59.0, 113.0, 1000.0, 780.0 ],
                        "boxes": [
                            {
                                "box": {
                                    "id": "obj-3",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 606.0, 271.0, 78.0, 22.0 ],
                                    "text": "openWindow"
                                }
                            },
                            {
                                "box": {
                                    "comment": "",
                                    "id": "obj-1",
                                    "index": 3,
                                    "maxclass": "inlet",
                                    "numinlets": 0,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 532.0, 36.0, 30.0, 30.0 ]
                                }
                            },
                            {
                                "box": {
                                    "fontname": "Arial",
                                    "fontsize": 13.0,
                                    "id": "obj-79",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 254.0, 170.0, 113.0, 21.0 ],
                                    "text": "Choose text font"
                                }
                            },
                            {
                                "box": {
                                    "fontname": "Arial",
                                    "fontsize": 13.0,
                                    "id": "obj-76",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 136.0, 206.0, 72.0, 23.0 ],
                                    "text": "textfont $1"
                                }
                            },
                            {
                                "box": {
                                    "allowdrag": 0,
                                    "fontsize": 13.0,
                                    "id": "obj-75",
                                    "items": [ "Ableton Sans", ",", "Ableton Sans Regular Italic", ",", "Ableton Sans ExtraLight", ",", "Ableton Sans ExtraLight Italic", ",", "Ableton Sans Light", ",", "Ableton Sans Light Italic", ",", "Ableton Sans Medium", ",", "Ableton Sans Medium Italic", ",", "Ableton Sans Bold", ",", "Ableton Sans Bold Italic", ",", "Ableton Sans Bold", ",", "Ableton Sans Light", ",", "Ableton Sans Medium", ",", "Ableton Sans Small", ",", "Ableton Sans Small Regular Italic", ",", "Ableton Sans Small Bold", ",", "Ableton Sans Small Bold Italic", ",", "Academico", ",", "Academico Italic", ",", "Academico Bold", ",", "Academico Bold Italic", ",", "Academy Engraved LET Plain", ",", "Al Bayan Plain", ",", "Al Bayan Bold", ",", "Al Nile", ",", "Al Nile Bold", ",", "Al Tarikh", ",", "AleBoxes", ",", "American Typewriter", ",", "American Typewriter Light", ",", "American Typewriter Semibold", ",", "American Typewriter Bold", ",", "American Typewriter Condensed", ",", "American Typewriter Condensed Light", ",", "American Typewriter Condensed Bold", ",", "Andale Mono", ",", "Apple Braille Outline 6 Dot", ",", "Apple Braille Outline 8 Dot", ",", "Apple Braille Pinpoint 6 Dot", ",", "Apple Braille Pinpoint 8 Dot", ",", "Apple Braille", ",", "Apple Chancery Chancery", ",", "Apple Color Emoji", ",", "Apple SD Gothic Neo", ",", "Apple SD Gothic Neo Thin", ",", "Apple SD Gothic Neo UltraLight", ",", "Apple SD Gothic Neo Light", ",", "Apple SD Gothic Neo Medium", ",", "Apple SD Gothic Neo SemiBold", ",", "Apple SD Gothic Neo Bold", ",", "Apple SD Gothic Neo ExtraBold", ",", "Apple SD Gothic Neo Heavy", ",", "Apple Symbols", ",", "AppleGothic", ",", "AppleMyungjo", ",", "Architects Daughter", ",", "Arial", ",", "Arial Italic", ",", "Arial Bold", ",", "Arial Bold Italic", ",", "Arial Black", ",", "Arial Hebrew", ",", "Arial Hebrew Light", ",", "Arial Hebrew Bold", ",", "Arial Hebrew Scholar", ",", "Arial Hebrew Scholar Light", ",", "Arial Hebrew Scholar Bold", ",", "Arial Narrow", ",", "Arial Narrow Italic", ",", "Arial Narrow Bold", ",", "Arial Narrow Bold Italic", ",", "Arial Rounded MT Bold", ",", "Arial Unicode MS", ",", "Avenir Book", ",", "Avenir Roman", ",", "Avenir Book Oblique", ",", "Avenir Oblique", ",", "Avenir Light", ",", "Avenir Light Oblique", ",", "Avenir Medium", ",", "Avenir Medium Oblique", ",", "Avenir Heavy", ",", "Avenir Heavy Oblique", ",", "Avenir Black", ",", "Avenir Black Oblique", ",", "Avenir Next", ",", "Avenir Next Italic", ",", "Avenir Next Ultra Light", ",", "Avenir Next Ultra Light Italic", ",", "Avenir Next Medium", ",", "Avenir Next Medium Italic", ",", "Avenir Next Demi Bold", ",", "Avenir Next Demi Bold Italic", ",", "Avenir Next Bold", ",", "Avenir Next Bold Italic", ",", "Avenir Next Heavy", ",", "Avenir Next Heavy Italic", ",", "Avenir Next Condensed", ",", "Avenir Next Condensed Italic", ",", "Avenir Next Condensed Ultra Light", ",", "Avenir Next Condensed Ultra Light Italic", ",", "Avenir Next Condensed Medium", ",", "Avenir Next Condensed Medium Italic", ",", "Avenir Next Condensed Demi Bold", ",", "Avenir Next Condensed Demi Bold Italic", ",", "Avenir Next Condensed Bold", ",", "Avenir Next Condensed Bold Italic", ",", "Avenir Next Condensed Heavy", ",", "Avenir Next Condensed Heavy Italic", ",", "Ayuthaya", ",", "Baghdad", ",", "Baka Too", ",", "Bangla MN", ",", "Bangla MN Bold", ",", "Bangla Sangam MN", ",", "Bangla Sangam MN Bold", ",", "Baskerville", ",", "Baskerville Italic", ",", "Baskerville SemiBold", ",", "Baskerville SemiBold Italic", ",", "Baskerville Bold", ",", "Baskerville Bold Italic", ",", "Bebas Neue", ",", "Beirut", ",", "BeStPlainmXZ", ",", "Beth Ellen", ",", "Big Caslon Medium", ",", "BigBand Chords", ",", "Bodoni 72 Book", ",", "Bodoni 72 Book Italic", ",", "Bodoni 72 Bold", ",", "Bodoni 72 Oldstyle Book", ",", "Bodoni 72 Oldstyle Book Italic", ",", "Bodoni 72 Oldstyle Bold", ",", "Bodoni 72 Smallcaps Book", ",", "Bodoni Ornaments", ",", "Boulez", ",", "Bradley Hand Bold", ",", "Bravura", ",", "Bravura", ",", "Bravura Text", ",", "Bravura Text", ",", "Brush Script MT Italic", ",", "Chalkboard", ",", "Chalkboard Bold", ",", "Chalkboard SE", ",", "Chalkboard SE Light", ",", "Chalkboard SE Bold", ",", "Chalkduster", ",", "Charis", ",", "Charis Italic", ",", "Charis Medium", ",", "Charis Medium Italic", ",", "Charis SemiBold", ",", "Charis SemiBold Italic", ",", "Charis Bold", ",", "Charis Bold Italic", ",", "Charter Roman", ",", "Charter Italic", ",", "Charter Bold", ",", "Charter Bold Italic", ",", "Charter Black", ",", "Charter Black Italic", ",", "Cochin", ",", "Cochin Italic", ",", "Cochin Bold", ",", "Cochin Bold Italic", ",", "Comic Sans MS", ",", "Comic Sans MS Bold", ",", "Copperplate", ",", "Copperplate Light", ",", "Copperplate Bold", ",", "Corsiva Hebrew", ",", "Corsiva Hebrew Bold", ",", "Courier New", ",", "Courier New Italic", ",", "Courier New Bold", ",", "Courier New Bold Italic", ",", "Crimson Roman", ",", "Crimson Italic", ",", "Crimson Semibold", ",", "Crimson SemiboldItalic", ",", "Crimson Bold", ",", "Crimson BoldItalic", ",", "Crimson Pro", ",", "Crimson Pro Italic", ",", "Crimson Pro ExtraLight", ",", "Crimson Pro ExtraLight Italic", ",", "Crimson Pro Light", ",", "Crimson Pro Light Italic", ",", "Crimson Pro Medium", ",", "Crimson Pro Medium Italic", ",", "Crimson Pro SemiBold", ",", "Crimson Pro SemiBold Italic", ",", "Crimson Pro Bold", ",", "Crimson Pro Bold Italic", ",", "Crimson Pro ExtraBold", ",", "Crimson Pro ExtraBold Italic", ",", "Crimson Pro Black", ",", "Crimson Pro Black Italic", ",", "Damascus", ",", "Damascus Light", ",", "Damascus Medium", ",", "Damascus Semi Bold", ",", "Damascus Bold", ",", "DecoType Naskh", ",", "Devanagari MT", ",", "Devanagari MT Bold", ",", "Devanagari Sangam MN", ",", "Devanagari Sangam MN Bold", ",", "Didot", ",", "Didot Italic", ",", "Didot Bold", ",", "DIN Alternate Bold", ",", "DIN Condensed Bold", ",", "Diwan Kufi", ",", "Diwan Thuluth", ",", "Euphemia UCAS", ",", "Euphemia UCAS Italic", ",", "Euphemia UCAS Bold", ",", "Farah", ",", "Farisi", ",", "Finale Ash", ",", "Finale Ash Text", ",", "Finale Broadway", ",", "Finale Broadway Text", ",", "Finale Jazz", ",", "Finale Jazz Text", ",", "Finale Jazz Text Lowercase", ",", "Finale Maestro", ",", "Finale Maestro Text", ",", "Finale Maestro Text Italic", ",", "Finale Maestro Text Bold", ",", "Finale Maestro Text Bold Italic", ",", "Futura Medium", ",", "Futura Medium", ",", "Futura Medium Italic", ",", "Futura Bold", ",", "Futura Condensed Medium", ",", "Futura Condensed ExtraBold", ",", "Galvji", ",", "Galvji Oblique", ",", "Galvji Bold", ",", "Galvji Bold Oblique", ",", "GB18030 Bitmap", ",", "Geeza Pro", ",", "Geeza Pro Bold", ",", "Geneva", ",", "Gentium", ",", "Gentium Italic", ",", "Gentium Medium", ",", "Gentium Medium Italic", ",", "Gentium SemiBold", ",", "Gentium SemiBold Italic", ",", "Gentium Bold", ",", "Gentium Bold Italic", ",", "Gentium ExtraBold", ",", "Gentium ExtraBold Italic", ",", "Georgia", ",", "Georgia Italic", ",", "Georgia Bold", ",", "Georgia Bold Italic", ",", "Gill Sans", ",", "Gill Sans Italic", ",", "Gill Sans Light", ",", "Gill Sans Light Italic", ",", "Gill Sans SemiBold", ",", "Gill Sans SemiBold Italic", ",", "Gill Sans Bold", ",", "Gill Sans Bold Italic", ",", "Gill Sans UltraBold", ",", "Golden Age", ",", "Grantha Sangam MN", ",", "Grantha Sangam MN Bold", ",", "Greifswaler Deutsche Schrift", ",", "Gujarati MT", ",", "Gujarati MT Bold", ",", "Gujarati Sangam MN", ",", "Gujarati Sangam MN Bold", ",", "Gurmukhi MN", ",", "Gurmukhi MN Bold", ",", "Gurmukhi MT", ",", "Gurmukhi Sangam MN", ",", "Gurmukhi Sangam MN Bold", ",", "Heiti SC Light", ",", "Heiti SC Medium", ",", "Heiti TC Light", ",", "Heiti TC Medium", ",", "Helsinki Metronome Std", ",", "Helsinki Special Std", ",", "Helsinki Std", ",", "Helsinki Text Std", ",", "Helvetica", ",", "Helvetica Oblique", ",", "Helvetica Light", ",", "Helvetica Light Oblique", ",", "Helvetica Bold", ",", "Helvetica Bold Oblique", ",", "Helvetica Neue", ",", "Helvetica Neue Italic", ",", "Helvetica Neue UltraLight", ",", "Helvetica Neue UltraLight Italic", ",", "Helvetica Neue Thin", ",", "Helvetica Neue Thin Italic", ",", "Helvetica Neue Light", ",", "Helvetica Neue Light Italic", ",", "Helvetica Neue Medium", ",", "Helvetica Neue Medium Italic", ",", "Helvetica Neue Bold", ",", "Helvetica Neue Bold Italic", ",", "Helvetica Neue Condensed Bold", ",", "Helvetica Neue Condensed Black", ",", "Herculanum", ",", "Hiragino Maru Gothic ProN W4", ",", "Hiragino Mincho ProN W3", ",", "Hiragino Mincho ProN W6", ",", "Hiragino Sans W0", ",", "Hiragino Sans W1", ",", "Hiragino Sans W2", ",", "Hiragino Sans W3", ",", "Hiragino Sans W4", ",", "Hiragino Sans W5", ",", "Hiragino Sans W6", ",", "Hiragino Sans W7", ",", "Hiragino Sans W8", ",", "Hiragino Sans W9", ",", "Hiragino Sans GB W3", ",", "Hiragino Sans GB W6", ",", "Hoefler Text", ",", "Hoefler Text Ornaments", ",", "Hoefler Text Italic", ",", "Hoefler Text Black", ",", "Hoefler Text Black Italic", ",", "IBM Plex Mono", ",", "IBM Plex Mono Italic", ",", "IBM Plex Mono Thin", ",", "IBM Plex Mono Thin Italic", ",", "IBM Plex Mono ExtraLight", ",", "IBM Plex Mono ExtraLight Italic", ",", "IBM Plex Mono Light", ",", "IBM Plex Mono Light Italic", ",", "IBM Plex Mono Medium", ",", "IBM Plex Mono Medium Italic", ",", "IBM Plex Mono SemiBold", ",", "IBM Plex Mono SemiBold Italic", ",", "IBM Plex Mono Bold", ",", "IBM Plex Mono Bold Italic", ",", "Impact", ",", "InaiMathi", ",", "InaiMathi Bold", ",", "Inkpen2 Chords Std", ",", "Inkpen2 Metronome Std", ",", "Inkpen2 Script Std", ",", "Inkpen2 Special Std", ",", "Inkpen2 Std", ",", "Inkpen2 Text Std", ",", "ITC Stone Sans Phonetic IPA", ",", "ITF Devanagari Book", ",", "ITF Devanagari Light", ",", "ITF Devanagari Medium", ",", "ITF Devanagari Demi", ",", "ITF Devanagari Bold", ",", "ITF Devanagari Marathi Book", ",", "ITF Devanagari Marathi Light", ",", "ITF Devanagari Marathi Medium", ",", "ITF Devanagari Marathi Demi", ",", "ITF Devanagari Marathi Bold", ",", "Kailasa", ",", "Kailasa Bold", ",", "Kannada MN", ",", "Kannada MN Bold", ",", "Kannada Sangam MN", ",", "Kannada Sangam MN Bold", ",", "Kefa", ",", "Kefa Bold", ",", "Khmer MN", ",", "Khmer MN Bold", ",", "Khmer Sangam MN", ",", "Kohinoor Bangla", ",", "Kohinoor Bangla Light", ",", "Kohinoor Bangla Medium", ",", "Kohinoor Bangla Semibold", ",", "Kohinoor Bangla Bold", ",", "Kohinoor Devanagari", ",", "Kohinoor Devanagari Light", ",", "Kohinoor Devanagari Medium", ",", "Kohinoor Devanagari Semibold", ",", "Kohinoor Devanagari Bold", ",", "Kohinoor Gujarati", ",", "Kohinoor Gujarati Light", ",", "Kohinoor Gujarati Medium", ",", "Kohinoor Gujarati Semibold", ",", "Kohinoor Gujarati Bold", ",", "Kohinoor Telugu", ",", "Kohinoor Telugu Light", ",", "Kohinoor Telugu Medium", ",", "Kohinoor Telugu Semibold", ",", "Kohinoor Telugu Bold", ",", "Kokonor", ",", "Krungthep", ",", "KufiStandardGK", ",", "Lao MN", ",", "Lao MN Bold", ",", "Lao Sangam MN", ",", "Lato", ",", "Lato", ",", "Lato Italic", ",", "Lato Italic", ",", "Lato Hairline", ",", "Lato Hairline Italic", ",", "Lato Thin", ",", "Lato Thin Italic", ",", "Lato Hairline", ",", "Lato Hairline Italic", ",", "Lato Thin", ",", "Lato Thin Italic", ",", "Lato Light", ",", "Lato Light", ",", "Lato Light Italic", ",", "Lato Light Italic", ",", "Lato Medium", ",", "Lato Medium", ",", "Lato Medium Italic", ",", "Lato Medium Italic", ",", "Lato Semibold", ",", "Lato Semibold", ",", "Lato Semibold Italic", ",", "Lato Semibold Italic", ",", "Lato Bold", ",", "Lato Bold", ",", "Lato Bold Italic", ",", "Lato Bold Italic", ",", "Lato Heavy", ",", "Lato Heavy", ",", "Lato Heavy Italic", ",", "Lato Heavy Italic", ",", "Lato Black", ",", "Lato Black", ",", "Lato Black Italic", ",", "Lato Black Italic", ",", "Leipzig", ",", "Leland", ",", "Leland", ",", "Leland Text", ",", "Libre Bodoni", ",", "Libre Bodoni Italic", ",", "Libre Bodoni Bold", ",", "Libre Bodoni Bold Italic", ",", "Lucida Grande", ",", "Lucida Grande Bold", ",", "Luminari", ",", "Malayalam MN", ",", "Malayalam MN Bold", ",", "Malayalam Sangam MN", ",", "Malayalam Sangam MN Bold", ",", "Mansalva", ",", "Marker Felt Thin", ",", "Marker Felt Wide", ",", "Menlo", ",", "Menlo Italic", ",", "Menlo Bold", ",", "Menlo Bold Italic", ",", "Microsoft Sans Serif", ",", "Mishafi", ",", "Mishafi Gold", ",", "Monaco", ",", "Mshtakan", ",", "Mshtakan Oblique", ",", "Mshtakan Bold", ",", "Mshtakan BoldOblique", ",", "Mukta Mahee", ",", "Mukta Mahee ExtraLight", ",", "Mukta Mahee Light", ",", "Mukta Mahee Medium", ",", "Mukta Mahee SemiBold", ",", "Mukta Mahee Bold", ",", "Mukta Mahee ExtraBold", ",", "Muna", ",", "Muna Bold", ",", "Muna Black", ",", "Myanmar MN", ",", "Myanmar MN Bold", ",", "Myanmar Sangam MN", ",", "Myanmar Sangam MN Bold", ",", "Myriad Pro", ",", "Nadeem", ",", "Nepomuk", ",", "Nepomuk Italic", ",", "Nepomuk SC", ",", "Nepomuk Bold SC", ",", "Nepomuk Bold", ",", "Nepomuk Bold Italic", ",", "New Peninim MT", ",", "New Peninim MT Inclined", ",", "New Peninim MT Bold", ",", "New Peninim MT Bold Inclined", ",", "Noteworthy Light", ",", "Noteworthy Bold", ",", "Noto Color Emoji SVG", ",", "Noto Nastaliq Urdu", ",", "Noto Nastaliq Urdu Bold", ",", "Noto Sans Batak", ",", "Noto Sans JP", ",", "Noto Sans JP Thin", ",", "Noto Sans JP Light", ",", "Noto Sans JP DemiLight", ",", "Noto Sans JP Medium", ",", "Noto Sans JP Bold", ",", "Noto Sans JP Black", ",", "Noto Sans Kannada", ",", "Noto Sans Kannada ExtraLight", ",", "Noto Sans Kannada Thin", ",", "Noto Sans Kannada Light", ",", "Noto Sans Kannada Medium", ",", "Noto Sans Kannada SemiBold", ",", "Noto Sans Kannada Bold", ",", "Noto Sans Kannada ExtraBold", ",", "Noto Sans Kannada Black", ",", "Noto Sans Myanmar", ",", "Noto Sans Myanmar ExtraLight", ",", "Noto Sans Myanmar Thin", ",", "Noto Sans Myanmar Light", ",", "Noto Sans Myanmar Medium", ",", "Noto Sans Myanmar SemiBold", ",", "Noto Sans Myanmar Bold", ",", "Noto Sans Myanmar ExtraBold", ",", "Noto Sans Myanmar Black", ",", "Noto Sans NKo", ",", "Noto Sans Oriya", ",", "Noto Sans Oriya Bold", ",", "Noto Sans Syriac", ",", "Noto Sans Syriac Thin", ",", "Noto Sans Syriac ExtraLight", ",", "Noto Sans Syriac Light", ",", "Noto Sans Syriac Medium", ",", "Noto Sans Syriac SemiBold", ",", "Noto Sans Syriac Bold", ",", "Noto Sans Syriac ExtraBold", ",", "Noto Sans Syriac Black", ",", "Noto Sans Tagalog", ",", "Noto Serif Myanmar", ",", "Noto Serif Myanmar ExtraLight", ",", "Noto Serif Myanmar Thin", ",", "Noto Serif Myanmar Light", ",", "Noto Serif Myanmar Medium", ",", "Noto Serif Myanmar SemiBold", ",", "Noto Serif Myanmar Bold", ",", "Noto Serif Myanmar ExtraBold", ",", "Noto Serif Myanmar Black", ",", "November for bach", ",", "Optima", ",", "Optima Italic", ",", "Optima Bold", ",", "Optima Bold Italic", ",", "Optima ExtraBlack", ",", "Opus Big Time Std", ",", "Opus Chords Sans Condensed Std", ",", "Opus Chords Sans Std", ",", "Opus Chords Std", ",", "Opus Figured Bass Extras Std", ",", "Opus Figured Bass Std", ",", "Opus Function Symbols Std", ",", "Opus Metronome Std", ",", "Opus Note Names Std", ",", "Opus Ornaments Std", ",", "Opus Percussion Std", ",", "Opus PlainChords Std", ",", "Opus Roman Chords Std", ",", "Opus Special Extra Std", ",", "Opus Special Std", ",", "Opus Std", ",", "Opus Text Std", ",", "Oriya MN", ",", "Oriya MN Bold", ",", "Oriya Sangam MN", ",", "Oriya Sangam MN Bold", ",", "Palatino", ",", "Palatino Italic", ",", "Palatino Bold", ",", "Palatino Bold Italic", ",", "Papyrus", ",", "Papyrus Condensed", ",", "Party LET Plain", ",", "Petaluma", ",", "Petaluma", ",", "Petaluma Script", ",", "Petaluma Text", ",", "PG Text Normal", ",", "PGChords", ",", "PGJazz", ",", "PGMusic F", ",", "PGTextj", ",", "PGTextje", ",", "Phosphate Inline", ",", "Phosphate Solid", ",", "PhotoScore Extra", ",", "PingFang HK", ",", "PingFang HK Ultralight", ",", "PingFang HK Thin", ",", "PingFang HK Light", ",", "PingFang HK Medium", ",", "PingFang HK Semibold", ",", "PingFang MO", ",", "PingFang MO Ultralight", ",", "PingFang MO Thin", ",", "PingFang MO Light", ",", "PingFang MO Medium", ",", "PingFang MO Semibold", ",", "PingFang SC", ",", "PingFang SC Ultralight", ",", "PingFang SC Thin", ",", "PingFang SC Light", ",", "PingFang SC Medium", ",", "PingFang SC Semibold", ",", "PingFang TC", ",", "PingFang TC Ultralight", ",", "PingFang TC Thin", ",", "PingFang TC Light", ",", "PingFang TC Medium", ",", "PingFang TC Semibold", ",", "Plantagenet Cherokee", ",", "Poppins Medium", ",", "PT Mono", ",", "PT Mono Bold", ",", "PT Sans", ",", "PT Sans Italic", ",", "PT Sans Bold", ",", "PT Sans Bold Italic", ",", "PT Sans Caption", ",", "PT Sans Caption Bold", ",", "PT Sans Narrow", ",", "PT Sans Narrow Bold", ",", "PT Serif", ",", "PT Serif Italic", ",", "PT Serif Bold", ",", "PT Serif Bold Italic", ",", "PT Serif Caption", ",", "PT Serif Caption Italic", ",", "Quicksand", ",", "Quicksand Light", ",", "Quicksand Medium", ",", "Quicksand SemiBold", ",", "Quicksand Bold", ",", "Raanana", ",", "Raanana Bold", ",", "RealScore Chords", ",", "Reprise Big Time Std", ",", "Reprise Chords Std", ",", "Reprise Metronome Std", ",", "Reprise Rehearsal Std", ",", "Reprise Script Std", ",", "Reprise Special Std", ",", "Reprise Stamp Std", ",", "Reprise Std", ",", "Reprise Text Std", ",", "Reprise Title Std", ",", "Roboto Slab", ",", "Rockwell", ",", "Rockwell Italic", ",", "Rockwell Bold", ",", "Rockwell Bold Italic", ",", "Rubik Mono One", ",", "Sana", ",", "Sathu", ",", "Savoye LET Plain", ",", "Savoye LET Plain", ",", "Saxophone", ",", "Sebastian", ",", "Sebastian Text", ",", "Shree Devanagari 714", ",", "Shree Devanagari 714 Italic", ",", "Shree Devanagari 714 Bold", ",", "Shree Devanagari 714 Bold Italic", ",", "SignPainter HouseScript", ",", "SignPainter HouseScript Semibold", ",", "Silom", ",", "Sinhala MN", ",", "Sinhala MN Bold", ",", "Sinhala Sangam MN", ",", "Sinhala Sangam MN Bold", ",", "Skia", ",", "Skia Light", ",", "Skia Bold", ",", "Skia Black", ",", "Skia Extended", ",", "Skia Light Extended", ",", "Skia Black Extended", ",", "Skia Condensed", ",", "Skia Light Condensed", ",", "Skia Black Condensed", ",", "Snell Roundhand", ",", "Snell Roundhand Bold", ",", "Snell Roundhand Black", ",", "Songti SC", ",", "Songti SC Light", ",", "Songti SC Bold", ",", "Songti SC Black", ",", "Songti TC", ",", "Songti TC Light", ",", "Songti TC Bold", ",", "Source Code Pro", ",", "Source Code Pro Italic", ",", "Source Code Pro ExtraLight", ",", "Source Code Pro ExtraLight Italic", ",", "Source Code Pro Light", ",", "Source Code Pro Light Italic", ",", "Source Code Pro Medium", ",", "Source Code Pro Medium Italic", ",", "Source Code Pro SemiBold", ",", "Source Code Pro SemiBold Italic", ",", "Source Code Pro Bold", ",", "Source Code Pro Bold Italic", ",", "Source Code Pro ExtraBold", ",", "Source Code Pro ExtraBold Italic", ",", "Source Code Pro Black", ",", "Source Code Pro Black Italic", ",", "Splentino", ",", "Splentino Italic", ",", "Splentino Bold", ",", "Splentino Bold Italic", ",", "Splentino Bold Condensed", ",", "STIX Two Math", ",", "STIX Two Text", ",", "STIX Two Text Italic", ",", "STIX Two Text Medium", ",", "STIX Two Text Medium Italic", ",", "STIX Two Text SemiBold", ",", "STIX Two Text SemiBold Italic", ",", "STIX Two Text Bold", ",", "STIX Two Text Bold Italic", ",", "STSong", ",", "Sukhumvit Set Text", ",", "Sukhumvit Set Light", ",", "Sukhumvit Set Medium", ",", "Sukhumvit Set Semi Bold", ",", "Sukhumvit Set Bold", ",", "Sukhumvit Set Thin", ",", "Symbol", ",", "Tahoma", ",", "Tahoma Bold", ",", "Tamil MN", ",", "Tamil MN Bold", ",", "Tamil Sangam MN", ",", "Tamil Sangam MN Bold", ",", "Telugu MN", ",", "Telugu MN Bold", ",", "Telugu Sangam MN", ",", "Telugu Sangam MN Bold", ",", "The Modern Chaos", ",", "Thonburi", ",", "Thonburi Light", ",", "Thonburi Bold", ",", "Times New Roman", ",", "Times New Roman Italic", ",", "Times New Roman Bold", ",", "Times New Roman Bold Italic", ",", "Tin Whistle Tab", ",", "Trajan Pro 3", ",", "Trattatello", ",", "Trebuchet MS", ",", "Trebuchet MS Italic", ",", "Trebuchet MS Bold", ",", "Trebuchet MS Bold Italic", ",", "Verdana", ",", "Verdana Italic", ",", "Verdana Bold", ",", "Verdana Bold Italic", ",", "Waseem", ",", "Waseem Light", ",", "Webdings", ",", "Wingdings", ",", "Wingdings 2", ",", "Wingdings 3", ",", "Yamaha EBM10 Bold Bold", ",", "Yamaha EBM15 Condensed Condensed Regular", ",", "Yamaha EBM7 Bold Bold", ",", "Yamaha EBM7 Regular", ",", "Yamaha EBM8 Expanded Expanded Regular", ",", "Yamaha EBM8 Regular", ",", "Zapf Dingbats", ",", "Zapfino" ],
                                    "maxclass": "umenu",
                                    "numinlets": 1,
                                    "numoutlets": 3,
                                    "outlettype": [ "int", "", "" ],
                                    "parameter_enable": 0,
                                    "patching_rect": [ 50.0, 170.0, 192.0, 23.0 ]
                                }
                            },
                            {
                                "box": {
                                    "fontname": "Arial",
                                    "fontsize": 13.0,
                                    "hidden": 1,
                                    "id": "obj-74",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "bang" ],
                                    "patching_rect": [ 50.0, 107.0, 65.0, 23.0 ],
                                    "text": "loadbang"
                                }
                            },
                            {
                                "box": {
                                    "fontname": "Arial",
                                    "fontsize": 13.0,
                                    "hidden": 1,
                                    "id": "obj-73",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 50.0, 140.0, 49.0, 23.0 ],
                                    "text": "fontlist"
                                }
                            },
                            {
                                "box": {
                                    "fontname": "Arial",
                                    "fontsize": 13.0,
                                    "id": "obj-23",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [ 254.0, 252.0, 118.0, 21.0 ],
                                    "text": "Choose music font"
                                }
                            },
                            {
                                "box": {
                                    "fontname": "Arial",
                                    "fontsize": 13.0,
                                    "id": "obj-45",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 136.0, 284.0, 85.0, 23.0 ],
                                    "text": "musicfont $1"
                                }
                            },
                            {
                                "box": {
                                    "allowdrag": 0,
                                    "fontsize": 13.0,
                                    "id": "obj-24",
                                    "items": [ "Bravura", ",", "Petaluma" ],
                                    "maxclass": "umenu",
                                    "numinlets": 1,
                                    "numoutlets": 3,
                                    "outlettype": [ "int", "", "" ],
                                    "parameter_enable": 0,
                                    "patching_rect": [ 50.0, 252.0, 192.0, 23.0 ]
                                }
                            },
                            {
                                "box": {
                                    "fontname": "Arial",
                                    "fontsize": 13.0,
                                    "id": "obj-61",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 205.0, 366.0, 97.0, 23.0 ],
                                    "text": "floatwindow $1"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-42",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 4,
                                    "outlettype": [ "", "", "", "newScore" ],
                                    "patching_rect": [ 418.0, 339.0, 67.0, 22.0 ],
                                    "text": "MaxScore",
                                    "varname": "MaxScore"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-52",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 4,
                                    "outlettype": [ "", "", "", "" ],
                                    "patching_rect": [ 418.0, 418.0, 136.0, 22.0 ],
                                    "text": "maxscore.canvas",
                                    "varname": "canvas"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-13",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 418.0, 197.0, 114.0, 22.0 ],
                                    "text": "importMusicXML $1"
                                }
                            },
                            {
                                "box": {
                                    "comment": "",
                                    "id": "obj-29",
                                    "index": 1,
                                    "maxclass": "inlet",
                                    "numinlets": 0,
                                    "numoutlets": 1,
                                    "outlettype": [ "int" ],
                                    "patching_rect": [ 205.0, 40.0, 30.0, 30.0 ]
                                }
                            },
                            {
                                "box": {
                                    "comment": "",
                                    "id": "obj-30",
                                    "index": 2,
                                    "maxclass": "inlet",
                                    "numinlets": 0,
                                    "numoutlets": 1,
                                    "outlettype": [ "" ],
                                    "patching_rect": [ 418.0, 36.0, 30.0, 30.0 ]
                                }
                            }
                        ],
                        "lines": [
                            {
                                "patchline": {
                                    "destination": [ "obj-42", 0 ],
                                    "midpoints": [ 541.5, 237.87890625, 427.5, 237.87890625 ],
                                    "source": [ "obj-1", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-42", 0 ],
                                    "source": [ "obj-13", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-45", 0 ],
                                    "source": [ "obj-24", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-61", 0 ],
                                    "source": [ "obj-29", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-42", 0 ],
                                    "source": [ "obj-3", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-13", 0 ],
                                    "source": [ "obj-30", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-52", 1 ],
                                    "midpoints": [ 443.5, 370.0, 544.5, 370.0 ],
                                    "source": [ "obj-42", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-52", 0 ],
                                    "source": [ "obj-42", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-42", 0 ],
                                    "midpoints": [ 145.5, 319.75, 427.5, 319.75 ],
                                    "source": [ "obj-45", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-42", 0 ],
                                    "midpoints": [ 427.5, 451.0, 405.0, 451.0, 405.0, 328.0, 427.5, 328.0 ],
                                    "source": [ "obj-52", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-52", 0 ],
                                    "midpoints": [ 214.5, 410.5, 427.5, 410.5 ],
                                    "source": [ "obj-61", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-75", 0 ],
                                    "hidden": 1,
                                    "source": [ "obj-73", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-42", 0 ],
                                    "midpoints": [ 59.5, 238.4375, 427.5, 238.4375 ],
                                    "order": 0,
                                    "source": [ "obj-74", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-73", 0 ],
                                    "hidden": 1,
                                    "order": 1,
                                    "source": [ "obj-74", 0 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-76", 0 ],
                                    "source": [ "obj-75", 1 ]
                                }
                            },
                            {
                                "patchline": {
                                    "destination": [ "obj-42", 0 ],
                                    "midpoints": [ 145.5, 239.5, 427.5, 239.5 ],
                                    "source": [ "obj-76", 0 ]
                                }
                            }
                        ]
                    },
                    "patching_rect": [ 40.0, 1141.0, 95.0, 22.0 ],
                    "text": "p canvas"
                }
            },
            {
                "box": {
                    "id": "obj-60",
                    "maxclass": "toggle",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "int" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 40.0, 1046.0, 24.0, 24.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-24",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 78.0, 1047.0, 73.0, 22.0 ],
                    "text": "route export"
                }
            },
            {
                "box": {
                    "id": "obj-10",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 886.0, 66.0, 82.0, 20.0 ],
                    "text": "MLP example"
                }
            },
            {
                "box": {
                    "id": "obj-15",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 593.0, 65.0, 283.0, 22.0 ],
                    "text": "read neural_counterpoint_soprano_bass.json, bang"
                }
            },
            {
                "box": {
                    "id": "obj-57",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 2148.0, 500.0, 50.0, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-13",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 496.0, 1235.0, 246.0, 20.0 ],
                    "text": "harmonic interval (for heuristic_all_positions)"
                }
            },
            {
                "box": {
                    "id": "obj-23",
                    "linecount": 3,
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 506.0, 1141.0, 125.0, 49.0 ],
                    "text": "1 12 14 22 11 17 19 18 14 14 1 5 11 11 5 9"
                }
            },
            {
                "box": {
                    "id": "obj-22",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 604.0, 1059.0, 87.0, 22.0 ],
                    "text": "bach.- @out m"
                }
            },
            {
                "box": {
                    "id": "obj-21",
                    "linecount": 2,
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 424.0, 1096.0, 149.0, 35.0 ],
                    "text": "64 60 62 57 63 62 57 63 62 57 63 62 63 60 64 62"
                }
            },
            {
                "box": {
                    "id": "obj-19",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 438.0, 1023.0, 125.0, 22.0 ],
                    "saved_object_attributes": {
                        "versionnumber": 80300
                    },
                    "text": "bach.pick 2 3 @out m"
                }
            },
            {
                "box": {
                    "id": "obj-18",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 438.0, 995.0, 74.0, 22.0 ],
                    "saved_object_attributes": {
                        "versionnumber": 80300
                    },
                    "text": "bach.flat 1 2"
                }
            },
            {
                "box": {
                    "id": "obj-12",
                    "linecount": 2,
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 318.0, 1059.0, 143.0, 35.0 ],
                    "text": "65 72 76 79 74 79 76 81 76 71 64 67 74 71 69 71"
                }
            },
            {
                "box": {
                    "id": "obj-8",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "patching_rect": [ 457.0, 14.0, 58.0, 22.0 ],
                    "text": "loadbang"
                }
            },
            {
                "box": {
                    "id": "obj-28",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 18.0, 981.0, 83.0, 22.0 ],
                    "text": "print solutions"
                }
            },
            {
                "box": {
                    "id": "obj-11",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 88.0, 904.0, 165.0, 22.0 ],
                    "text": "config_dict_debug demo_dict"
                }
            },
            {
                "box": {
                    "id": "obj-47",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "outlettype": [ "bang", "" ],
                    "patching_rect": [ 17.0, 40.0, 46.0, 22.0 ],
                    "text": "sel 118"
                }
            },
            {
                "box": {
                    "id": "obj-48",
                    "maxclass": "newobj",
                    "numinlets": 0,
                    "numoutlets": 4,
                    "outlettype": [ "int", "int", "int", "int" ],
                    "patching_rect": [ 17.0, 14.0, 50.5, 22.0 ],
                    "text": "key"
                }
            },
            {
                "box": {
                    "id": "obj-9",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 64.0, 871.0, 125.0, 22.0 ],
                    "text": "config_dict demo_dict"
                }
            },
            {
                "box": {
                    "id": "obj-6",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 5,
                    "outlettype": [ "dictionary", "", "", "", "" ],
                    "patching_rect": [ 64.0, 842.0, 86.0, 22.0 ],
                    "saved_object_attributes": {
                        "legacy": 0,
                        "parameter_enable": 0,
                        "parameter_mappable": 0
                    },
                    "text": "dict demo_dict"
                }
            },
            {
                "box": {
                    "code": "{\n    \"name\": \"Neural Folk Counterpoint — Soprano + Bass, 16 Positions\",\n    \"description\": \"2-voice counterpoint with neural scorer and minimal hard constraints — designed to maximise neural influence visibility. Hard rules: soprano above bass, max two-octave spread, soprano max leap P5, no adjacent repeat in soprano, isorhythm. Soft rules (heuristic:true, always active): prefer consonant harmony, avoid parallel 5ths/8ths, prefer perfect consonances on downbeats, prefer contrary motion. The reduced hard-constraint set leaves ~8–12 candidates per soprano step after propagation, giving the neural folk-melody MLP clear room to rank them by trained probability.\",\n    \"solution_length\": 16,\n    \"num_voices\": 2,\n    \"voices\": [\n        {\n            \"id\": 0,\n            \"description\": \"Soprano — C major pitches C4–A5, neural-guided\",\n            \"rhythm\": {\n                \"duration_values\": [ \"1/4\" ]\n            },\n            \"pitch\": {\n                \"midi_values\": [ 60, 61, 62, 63, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83 ]\n            }\n        },\n        {\n            \"id\": 1,\n            \"description\": \"Bass — C major pitches A2–E4, harmonic support\",\n            \"rhythm\": {\n                \"duration_values\": [ \"1/4\" ]\n            },\n            \"pitch\": {\n                \"midi_values\": [ 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64 ]\n            }\n        }\n    ],\n    \"search_options\": {\n        \"engine\": \"dfs\",\n        \"enable_metric_engine\": 0,\n        \"random_seed\": 0,\n        \"timeout_ms\": 30000,\n        \"branching\": \"first_fail\",\n        \"value_order\": \"neural\",\n        \"neural_weights_file\": \"weights/harmonic_weights.json\",\n        \"neural_temperature\": 1.0\n    },\n    \"export_json\": 0,\n    \"export_txt\": 0,\n    \"export_xml\": 1,\n    \"file_name\": \"output/neural_counterpoint_soprano_bass\",\n    \"rules\": [\n        {\n            \"id\": \"soprano_above_bass\",\n            \"rule_type\": \"r-pitch-pitch\",\n            \"constraint\": \"voice_above\",\n            \"target_voices\": [ 0, 1 ],\n            \"description\": \"Hard: soprano strictly above bass at every position\"\n        },\n        {\n            \"id\": \"consonant_harmony\",\n            \"rule_type\": \"r-pitch-pitch\",\n            \"constraint\": \"interval_class\",\n            \"parameters\": [ 0, 3, 4, 7, 8, 9 ],\n            \"target_voices\": [ 0, 1 ],\n            \"enabled\": 0,\n            \"description\": \"Disabled: was masking neural harmonic signal. Soft version (soft_consonant_harmony) still applies as a heuristic.\"\n        },\n        {\n            \"id\": \"max_spacing_two_octaves\",\n            \"rule_type\": \"r-pitch-pitch\",\n            \"constraint\": \"max_interval\",\n            \"parameters\": [ 24 ],\n            \"target_voices\": [ 0, 1 ],\n            \"description\": \"Hard: voices may not be more than two octaves (24 semitones) apart\"\n        },\n        {\n            \"id\": \"soprano_max_leap_fifth\",\n            \"rule_type\": \"wildcard_constraint\",\n            \"wildcard_type\": \"for_all_voices\",\n            \"pattern_offsets\": [ 0, 1 ],\n            \"constraint\": \"abs(voice[v].pitch[i+1] - voice[v].pitch[i]) <= 7\",\n            \"target_voices\": [ 0 ],\n            \"target_component\": \"pitch\",\n            \"description\": \"Hard: soprano max leap P5 (7 semitones) -- leaves ~8-12 candidates per step for the neural scorer\"\n        },\n        {\n            \"id\": \"soprano_no_adjacent_repeat\",\n            \"rule_type\": \"wildcard_constraint\",\n            \"wildcard_type\": \"for_all_voices\",\n            \"pattern_offsets\": [ 0, 1 ],\n            \"constraint\": \"voice[v].pitch[i] != voice[v].pitch[i+1]\",\n            \"target_voices\": [ 0 ],\n            \"target_component\": \"pitch\",\n            \"description\": \"Hard: no immediately repeated pitches in soprano\"\n        },\n        {\n            \"id\": \"isorhythm_voices\",\n            \"rule_type\": \"r-rhythm-rhythm\",\n            \"constraint\": \"isorhythm\",\n            \"target_voices\": [ 0, 1 ],\n            \"description\": \"Hard: both voices share identical rhythm values\"\n        },\n        {\n            \"id\": \"no_simultaneous_rests\",\n            \"rule_type\": \"r-rhythm-rhythm\",\n            \"constraint\": \"no_simultaneous_rests\",\n            \"target_voices\": [ 0, 1 ],\n            \"description\": \"Hard: at least one voice sounding at every position\"\n        },\n        {\n            \"id\": \"soft_consonant_harmony\",\n            \"rule_type\": \"r-pitch-pitch\",\n            \"constraint\": \"interval_class\",\n            \"parameters\": [ 0, 3, 4, 7, 8, 9 ],\n            \"target_voices\": [ 0, 1 ],\n            \"heuristic\": 1,\n            \"description\": \"Soft: prefer consonant intervals -- never rejects, only ranks consonant options higher\"\n        },\n        {\n            \"id\": \"soft_no_parallel_fifths\",\n            \"rule_type\": \"r-pitch-pitch\",\n            \"constraint\": \"no_consecutive_fifths\",\n            \"target_voices\": [ 0, 1 ],\n            \"heuristic\": 1,\n            \"description\": \"Soft: prefer to avoid parallel fifths\"\n        },\n        {\n            \"id\": \"soft_no_parallel_octaves\",\n            \"rule_type\": \"r-pitch-pitch\",\n            \"constraint\": \"no_consecutive_octaves\",\n            \"target_voices\": [ 0, 1 ],\n            \"heuristic\": 1,\n            \"description\": \"Soft: prefer to avoid parallel octaves\"\n        },\n        {\n            \"id\": \"prefer_perfect_on_downbeats\",\n            \"rule_type\": \"r-pitch-pitch\",\n            \"constraint\": \"interval_class\",\n            \"parameters\": [ 0, 7 ],\n            \"indices\": [ 0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60 ],\n            \"target_voices\": [ 0, 1 ],\n            \"heuristic\": 1,\n            \"description\": \"Soft: prefer perfect consonances on every downbeat (every 4th position)\"\n        },\n        {\n            \"id\": \"prefer_contrary_motion\",\n            \"rule_type\": \"r-pitch-pitch\",\n            \"constraint\": \"contrary_motion\",\n            \"target_voices\": [ 0, 1 ],\n            \"heuristic\": 1,\n            \"description\": \"Soft: prefer contrary motion between voices\"\n        }\n    ],\n    \"dynamic_rules\": [],\n    \"harmonic_domain\": [\n        {\n            \"beat\": 0,\n            \"chord\": \"C\",\n            \"quality\": \"major\"\n        },\n        {\n            \"beat\": 4,\n            \"chord\": \"F\",\n            \"quality\": \"major\"\n        },\n        {\n            \"beat\": 8,\n            \"chord\": \"G\",\n            \"quality\": \"dom7\"\n        },\n        {\n            \"beat\": 12,\n            \"chord\": \"C\",\n            \"quality\": \"major\"\n        }\n    ]\n}",
                    "editlocked": 1,
                    "fontface": 0,
                    "fontname": "<Monospaced>",
                    "fontsize": 12.0,
                    "id": "obj-1",
                    "maxclass": "dict.codebox",
                    "numinlets": 2,
                    "numoutlets": 5,
                    "outlettype": [ "dictionary", "", "", "", "" ],
                    "patching_rect": [ 64.0, 178.0, 963.0, 651.0 ],
                    "saved_object_attributes": {
                        "legacy": 0,
                        "parameter_enable": 0,
                        "parameter_mappable": 0
                    }
                }
            },
            {
                "box": {
                    "id": "obj-solve",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 17.0, 67.0, 45.0, 22.0 ],
                    "text": "solve"
                }
            },
            {
                "box": {
                    "id": "obj-status",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 81.0, 69.0, 48.0, 22.0 ],
                    "text": "status"
                }
            },
            {
                "box": {
                    "id": "obj-getlast",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 140.0, 69.0, 62.0, 22.0 ],
                    "text": "get_last"
                }
            },
            {
                "box": {
                    "id": "obj-cancel",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 213.0, 69.0, 52.0, 22.0 ],
                    "text": "cancel"
                }
            },
            {
                "box": {
                    "id": "obj-solver",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "", "", "" ],
                    "patching_rect": [ 18.0, 946.0, 108.0, 22.0 ],
                    "text": "gecode.solver"
                }
            },
            {
                "box": {
                    "id": "obj-print-status",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 107.0, 981.0, 106.0, 22.0 ],
                    "text": "print solver_status"
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "obj-6", 0 ],
                    "source": [ "obj-1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 97.5, 939.5, 27.5, 939.5 ],
                    "source": [ "obj-11", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-1", 0 ],
                    "midpoints": [ 602.5, 130.17578125, 73.5, 130.17578125 ],
                    "source": [ "obj-15", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-19", 0 ],
                    "source": [ "obj-18", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-12", 1 ],
                    "order": 1,
                    "source": [ "obj-19", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-21", 1 ],
                    "order": 1,
                    "source": [ "obj-19", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-22", 1 ],
                    "midpoints": [ 553.5, 1052.0, 681.5, 1052.0 ],
                    "order": 0,
                    "source": [ "obj-19", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-22", 0 ],
                    "midpoints": [ 447.5, 1052.0, 613.5, 1052.0 ],
                    "order": 0,
                    "source": [ "obj-19", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-23", 1 ],
                    "source": [ "obj-22", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-31", 1 ],
                    "source": [ "obj-24", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-26", 0 ],
                    "order": 0,
                    "source": [ "obj-25", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-60", 0 ],
                    "midpoints": [ 179.5, 1042.25, 49.5, 1042.25 ],
                    "order": 1,
                    "source": [ "obj-25", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-31", 2 ],
                    "midpoints": [ 179.5, 1076.875, 125.5, 1076.875 ],
                    "source": [ "obj-26", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-1", 0 ],
                    "source": [ "obj-45", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solve", 0 ],
                    "source": [ "obj-47", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-47", 0 ],
                    "source": [ "obj-48", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-9", 0 ],
                    "source": [ "obj-6", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-31", 0 ],
                    "source": [ "obj-60", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-15", 0 ],
                    "midpoints": [ 466.5, 47.58203125, 602.5, 47.58203125 ],
                    "source": [ "obj-8", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 73.5, 921.5, 27.5, 921.5 ],
                    "source": [ "obj-9", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 222.5, 114.5, 27.5, 114.5 ],
                    "source": [ "obj-cancel", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 149.5, 114.5, 27.5, 114.5 ],
                    "source": [ "obj-getlast", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "source": [ "obj-solve", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-18", 0 ],
                    "midpoints": [ 27.5, 974.5, 447.5, 974.5 ],
                    "order": 0,
                    "source": [ "obj-solver", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-24", 0 ],
                    "source": [ "obj-solver", 2 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-28", 0 ],
                    "order": 1,
                    "source": [ "obj-solver", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-print-status", 0 ],
                    "source": [ "obj-solver", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-solver", 0 ],
                    "midpoints": [ 90.5, 114.5, 27.5, 114.5 ],
                    "source": [ "obj-status", 0 ]
                }
            }
        ],
        "parameters": {
            "obj-31::obj-52::obj-31": [ "live.tab[29]", "live.tab", 0 ],
            "obj-31::obj-52::obj-38": [ "live.tab[3]", "live.tab", 0 ],
            "parameterbanks": {
                "0": {
                    "index": 0,
                    "name": "",
                    "parameters": [ "-", "-", "-", "-", "-", "-", "-", "-" ],
                    "buttons": [ "-", "-", "-", "-", "-", "-", "-", "-" ]
                }
            },
            "inherited_shortname": 1
        },
        "autosave": 0
    }
}