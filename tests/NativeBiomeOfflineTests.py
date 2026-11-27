"""Run with a fixture emitted by NativeBiomeGeometryTests: standard library only."""
import importlib.util
import pathlib
import subprocess
import sys
import tempfile
import unittest
from xml.etree import ElementTree

ROOT=pathlib.Path(__file__).resolve().parents[1]
TOOL=ROOT/'tools'/'Analyze-NativeBiomeAtlas.py'
spec=importlib.util.spec_from_file_location('native_biome_offline',TOOL)
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
FIXTURE=pathlib.Path(sys.argv[1]).resolve()
sys.argv=sys.argv[:1]

class OfflineTests(unittest.TestCase):
    def setUp(self):
        self.records=mod.read_geometry(FIXTURE)

    def test_native_export_matches_cpp_fixture(self):
        self.assertEqual(len(self.records),5)
        q=mod.query(self.records,60,5,5)
        self.assertEqual((q['status'],q['biome'],q['broadIds'],q['localIds']),('unique-biome','snow','00000001,00000002','00000003'))
        self.assertEqual(mod.query(self.records,61,5,5)['biome'],'coastal')
        self.assertEqual(mod.query(self.records,62,5,5)['status'],'no-biome-coverage')
        self.assertEqual(mod.query(self.records,60,0,5)['status'],'biome-boundary')
        self.assertEqual(self.records[0]['editorId'],'Example, "record"')

    def test_conflict_and_incomplete_abstain(self):
        self.records[1]['biome']='coastal'
        self.assertEqual(mod.query(self.records,60,5,5)['status'],'conflicting-biomes')
        self.records[1]['incomplete']=True
        self.assertEqual(mod.query(self.records,60,100,100)['status'],'incomplete-biome-geometry')

    def test_truncated_export_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d)/'truncated.csv'
            p.write_text('\n'.join(FIXTURE.read_text().splitlines()[:-1]),encoding='utf-8')
            with self.assertRaises(ValueError): mod.read_geometry(p)

    def test_end_to_end_report(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d); replay=p/'replay.csv'; output=p/'report'
            replay.write_text('label,worldspace,x,y,observed,expectedBiome\nfixture,60,5,5,synthetic,snow\n',encoding='utf-8')
            cmd=[sys.executable,str(TOOL),str(FIXTURE),'--replay',str(replay),'--output',str(output)]
            subprocess.run(cmd,check=True,capture_output=True)
            ElementTree.parse(output/'native-biome-map.svg')
            self.assertIn('unique-biome,snow',(output/'native-biome-replay.csv').read_text())
            self.assertIn('"recordBounds"',(output/'native-biome-summary.json').read_text())
            self.assertNotEqual(subprocess.run(cmd,capture_output=True).returncode,0) # no overwrites

if __name__=='__main__': unittest.main()
