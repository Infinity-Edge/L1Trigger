Software setup
```
cmsrel CMSSW_10_5_0_pre1
cd CMSSW_10_5_0_pre1/src
cmsenv
git cms-init
git cms-checkout-topic -u p2l1pfp:L1PF_10_5_X_v3

# scripts
git clone git@github.com:p2l1pfp/FastPUPPI.git -b 105X_v3

# Replace L1Trigger directory
rm -rf L1Trigger
git clone git@github.com:Infinity-Edge/L1Trigger.git

scram b clean
scram b -j8
```

Produce NTuple

Input files path on the Lxplus: /eos/cms/store/cmst3/user/gpetrucc/l1tr/105X/NewInputs104X/010319/

To produce NTuple (e.g. ttbar PU200 sample)
```
./scripts/prun.sh runPerformanceNTuple.py --v3 TTbar_PU200 TTbar_PU200
```

If you want to add other algorithm outputs, please use below command
```
./scripts/prun.sh runPerformanceNTuple.py --v3 TTbar_PU200 TTbar_PU200 --inline-customize 'addCHS();addPuppiOld();addTKs()'
```
