#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@author: jacobfaibussowitsch
"""

import os

class Runner(object):
  def __init__(self,*args):
    self.argList = list(args)
    self.setuptime = {}
    self.solvetime = {}
    self.itercount = {}
    return

  def parseOutput(self,out,device,smoother):
    import re

    refelems = re.compile('^Number of finite element unknowns: ([0-9]+)')
    refsetup = re.compile('^Setup time = ([0-9]+\.[0-9]+)')
    refsolve = re.compile('^Solve time = ([0-9]+\.[0-9]+)')
    refiter  = re.compile('\s+Iteration :\s+([0-9]+)')

    if device not in self.setuptime:
      self.setuptime[device] = {}
    if device not in self.solvetime:
      self.solvetime[device] = {}
    if device not in self.itercount:
      self.itercount[device] = {}

    for line in out.split('\n'):
      elems = refelems.search(line)
      if elems:
        self.elems = elems.group(1)
        continue
      setuptime = refsetup.search(line)
      if setuptime:
        self.setuptime[device] = {smoother : setuptime.group(1)}
        continue
      solvetime = refsolve.search(line)
      if solvetime:
        self.solvetime[device] = {smoother : solvetime.group(1)}
        continue
      itercnt = refiter.search(line)
      if itercnt:
        cnt = itercnt.group(1)

    self.itercount[device] = {smoother : cnt}
    return

  def run(self):
    import subprocess

    for device in ['cpu','cuda']:
      argListBase = self.argList+['--device',device]
      for smoother in ['J','DR']:
        argList = argListBase+['--smoother',smoother]
        argStr = ' '.join(argList)
        print('Running {}'.format(argStr),end='\t',flush=True)
        with subprocess.Popen(argList,stdout=subprocess.PIPE,stderr=subprocess.PIPE) as runner:
          (out,err) = runner.communicate()
          assert not runner.returncode,'{} raised returncode {}:\nstdout: {}\nstderr:{}'.format(argStr,runner.returncode,out.decode(),err.decode())
        self.parseOutput(out.decode(),device,smoother)
        print('success')
    return

  def get(self):
    return (self.setuptime,self.solvetime,self.itercount)

def main(exe,ordRange,refine,mesh):
  assert os.path.exists(exe), 'Could not find {}'.format(exe)

  results = []
  for order in ordRange:
    runner = Runner(exe,'-o',str(order),'-r',str(refine),'-no-vis','--mesh',mesh)
    runner.run()
    results.append(runner.get())
    print(results)
  return


if __name__ == '__main__':
  import argparse

  parser = argparse.ArgumentParser(description='Collect timing results for DRSmoothers',formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('exec',help='path to example to time')
  parser.add_argument('-o','--order-range',metavar='int',default=[4,9],type=int,nargs=2,dest='ordrange')
  parser.add_argument('-r','--refine',metavar='int',default=5,type=int,dest='refine')
  parser.add_argument('-m','--mesh',default='../data/inline-hex.mesh')
  args = parser.parse_args()

  main(os.path.abspath(args.exec),range(*args.ordrange),args.refine,args.mesh)
