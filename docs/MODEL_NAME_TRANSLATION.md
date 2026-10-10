# Model name translation

The Help menu shows the model-language preference as
`Current model default language: Auto`. Clicking it cycles through Auto,
Japanese, and English. The next item, `Model language auto-translation`, toggles
dictionary translation for displayed bone, morph, and recognized display-group
names. It defaults to off.

| Preference | Translation target |
| --- | --- |
| Auto with English UI | English dictionary column |
| Auto with Japanese UI | Japanese dictionary column |
| Auto with any other UI language, including Chinese | Japanese dictionary column |
| Japanese | Japanese dictionary column |
| English | English dictionary column |

Changing either setting refreshes the main lists and timeline. Names in bone
selection, physics, curves, and channel selection use the same display helpers.
Both settings persist in `Data/model_info_language.ini`; existing files with
only a language value remain valid and start with translation off.

## Dictionary and name identity

The built-in tables come from the supplied references in `D:/MMD-Next/refs/`:

- `骨骼转英文.txt`: 446 entries in `src/main/bone_name_dictionary.inc`.
- `表情命名 日转英参考字典.txt`: 410 entries in
  `src/main/morph_name_dictionary.inc`.

Each table retains source order, Japanese and English spellings, and optional
third-column aliases. The first entry wins when a source repeats a spelling.
Exact Japanese names take precedence over English aliases; English matching
also accepts ASCII case differences. Matches do not remove custom suffixes or
normalize punctuation. If the preferred model name has no match, the alternate
language field is checked. Unmapped names remain as displayed before translation.

Translation changes labels only. It does not overwrite model names, bone or
morph identities, saved files, motion matching, or plugin interfaces. Model
titles have no supplied dictionary and keep their existing language-field
selection and Unicode fallback.

## Verification on 2026-10-10

Release x64 tests cover all three UI languages, all three model-language
preferences, translation on/off, English-to-Japanese lookup, supplied spelling
and left/right conventions, duplicate entries, unmapped Unicode names, and
unchanged raw identities. Hidden native combo boxes exercise production bone
and morph list population. Both shipped menu resources are checked for captions,
adjacent placement, enabled state, and check marks. Preference tests use an
isolated temporary directory and cover saving, reloading, old files, and absent
files.

`ui_language_tests` and `menu_language_regressions` pass. A test executable copy
with a Japanese process manifest also passes the complete UI suite and the
earlier PMX-import regressions with `ACP=932`. The host Windows locale is not
changed. See [the original Unicode repair](UI_JAPANESE_LOCALE_NAMES.md) for
before/after failure evidence. No rendered-scene FPS or GPU comparison was run;
performance remains unverified.

## Retained local outputs

- `.local/artifacts/model-name-translation/01/`: build and test logs,
  dictionary hashes, final source revision, build options, worktree status,
  and executable hashes.
- `.local/worktrees/ui-japanese-names/`: isolated worktree reused for the final
  Unicode repair and translation build.
- `.local/releases/model-name-translation-after/`: final binary snapshot.
- `.local/archives/ui-japanese-names/`: previously installed executable.
- `build/Release/MikuMikuDance.exe`: final installed executable.

Earlier baseline and Unicode-only snapshots remain in the locations recorded
by the original repair. These local outputs are retained, not published or
tagged as a user-accepted release.
