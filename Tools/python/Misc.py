#!/usr/bin/env python
'''@package docstring
Some common python functions
'''

from re import sub
from sys import stdout,stderr
from os import getenv
from collections import namedtuple

def PInfo(module,msg):
    ''' function to write to stdout'''
    stdout.write('\033[0;32mINFO\033[0m    [%-40s]: %s\n'%(module,msg))

def PWarning(module,msg):
    ''' function to write to stdout'''
    stdout.write('\033[0;91mWARNING\033[0m [%-40s]: %s\n'%(module,msg))

def PDebug(module,msg):
    ''' function to write to stdout'''
    stderr.write('\033[0;36mDEBUG\033[0m   [%-40s]: %s\n'%(module,msg))

def PError(module,msg):
    ''' function to write to stdout'''
    stderr.write('\033[0;41mERROR\033[0m   [%-40s]: %s\n'%(module,msg))


ModelParams = namedtuple('ModelParams',['m_V','m_DM','gV_DM','gA_DM','gV_q','gA_q','sigma','delta'])

def read_nr_model(mV,mDM,couplings=None):
    tmpl = getenv('PANDA_XSECS')+'/non-resonant/%i_%i_xsec_gencut.dat'
    try:
        fdat = open(tmpl%(mV,mDM))
    except IOError:
        PError('PandaCore.Tools.Misc.read_nr_model','Could not open %s'%(tmpl%(mV,mDM)))
        return None
    for line in fdat:
        if 'med dm' in line:
            continue
        p = ModelParams(*[float(x) for x in line.strip().split()])
        if couplings:
            if couplings==(p.gV_DM,p.gA_DM,p.gV_q,p.gA_q):
                fdat.close()
                return p
            else:
                continue
        else:
            # if not specified, take the first valid model (nominal)
            fdat.close()
            return p

def read_r_model(mV,mDM=100,couplings='nominal'):
    tmpl = getenv('PANDA_XSECS')+'/resonant/%i_%i.dat'
    try:
        fdat = open(tmpl%(mV,mDM))
    except IOError:
        PError('PandaCore.Tools.Misc.read_nr_model','Could not open %s'%(tmpl%(mV,mDM)))
        return None
    for line in fdat:
        line_coupling,sigma = line.split(':')
        if not(line_coupling==couplings):
            continue
        sigma = float(sigma)
        p = ModelParams(mV,mDM,1,1,0.25,0.25,sigma,0)
        fdat.close()
        return p 


def setBins(dist,bins):
    ''' Given a list of bin edges, sets them for a PlotUtility::Distribution DEPRECATED'''
    for b in bins:
        dist.AddBinEdge(b)

def tAND(s1,s2):
    ''' ANDs two strings '''
    return "(( "+s1+" ) && ( "+s2+" ))"

def tOR(s1,s2):
    ''' ORs two strings'''
    return "(( "+s1+" ) || ( "+s2+" ))"

def tTIMES(w,s):
    ''' MULTIPLIES two strings'''
    return "( "+w+" ) * ( "+s+" )"

def tNOT(w):
    ''' NOTs two strings'''
    return '!( '+w+' )'

def removeCut(basecut,var):
    ''' 
    Removes the dependence on a particular variable from a formula

    @type basecut: str
    @param basecut: Input formula to modify

    @type var: str
    @param var: Variable to remove from basecut

    @rtype: string
    @return: Returns a formula with the var dependence removed
    '''
    return sub('[0-9\.]*[=<>]*%s'%(var.replace('(','\(').replace(')','\)')),
               '1==1',
                 sub('%s[=<>]+[0-9\.]+'%(var.replace('(','\(').replace(')','\)')),
                     '1==1',
                     basecut)
                 )

