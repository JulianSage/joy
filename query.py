#!/usr/bin/python2.7

# query.py implements flow filtering and data selection functions
#
# see the "usage" function for instructions
#
# TBD:
# compute entropy from bd
# string matching functions
# compound filter statements 
# select internal traffic 
# select on sa/da labels
# allow port = sp/dp, addr = sa/da, etc.
#
# distinct | counted | orderby
# 
# list elements 
#
# functions:
#    entropy(bd)
#    reo2(bd)
#    max(bd)
#    min(bd)
#    avg(bd)
#    var(bd)
#
# functions can be applied to the elements in the predicate, or to the output data
#
# output can be flows, or an array of json objects, or a single json object  
#  
# pretty-printing function for flows
# names to numbers output translation (dp=80 -> dp=HTTP)
# numbers to names input translation (dp=HTTP -> dp=80,dp=8080)


import sys, json, operator
from optparse import OptionParser
from pprint import pprint
from math import sqrt

class flowstats:
   def __init__(self):
      self.numbytes = 0
      self.num_msg = 0
      self.numflows = 0
      self.inbytes = 0
      self.outbytes = 0
      self.inmsg = 0
      self.outmsg = 0
      self.inbytesSq = 0
      self.outbytesSq = 0
      self.numbytesSq = 0
      self.lengths = {}
      self.times = {}
      self.rle_lengths = {}

   def observe(self, numbytes, direction, time):
      self.numbytes += numbytes
      self.numbytesSq += numbytes * numbytes
      self.num_msg += 1
      if direction == ">":
         self.outbytes += numbytes
         self.outbytesSq += numbytes * numbytes
         self.outmsg += 1
      else:
         self.inbytes += numbytes
         self.inbytesSq += numbytes * numbytes
         self.inmsg += 1
      if numbytes not in self.lengths:
         self.lengths[numbytes] = 1
      else:
         self.lengths[numbytes] = self.lengths[numbytes] + 1
      if time not in self.times:
         self.times[time] = 1
      else:
         self.times[time] = self.times[time] + 1

   def print_lengths(self):
      for x in self.lengths:
        print str(self.lengths[x]) + "\t" + str(x)
      # for x in self.rle_lengths:
      #   print str(self.rle_lengths[x]) + "\t" + str(x)

   def print_times(self):
      for x in self.times:
        print str(self.times[x]) + "\t" + str(x)
      # for x in self.rle_lengths:
      #   print str(self.rle_lengths[x]) + "\t" + str(x)

   def printflowstats(self):
      print "flows:      " + '%5s' % str(self.numflows)
      print "messages:   " + '%5s' % str(self.num_msg)
      print "bytes:      " + '%5s' % str(self.numbytes)
      print "> messages: " + '%5s' % str(self.outmsg)
      print "> bytes:    " + '%5s' % str(self.outbytes)
      print "< messages: " + '%5s' % str(self.inmsg)
      print "< bytes:    " + '%5s' % str(self.inbytes)
      if self.numflows > 0:
         amf = float(self.num_msg)/float(self.numflows)
         print "messages per flow:    " + '%5s' % str(amf)
         afs = float(self.numbytes)/float(self.numflows)
         print "bytes per flow:       " + '%5s' % str(afs) 
         amf = float(self.outmsg)/float(self.numflows)
         print "outbound messages per flow: " + '%5s' % str(amf)
         amf = float(self.inmsg)/float(self.numflows)
         print "inbound messages per flow:  " + '%5s' % str(amf)
      if self.num_msg > 1:
         ads = float(self.numbytes)/float(self.num_msg)
         print "average message size: " + '%5s' % str(ads)
         vms = (float(self.numbytesSq) - float(self.numbytes * self.numbytes)/float(self.num_msg))/float(self.num_msg - 1)
         print "std dev message size: " + '%5s' % str(sqrt(vms))
      if self.inmsg > 1:
         ads = float(self.inbytes)/float(self.inmsg)
         print "average inbound message size: " + '%5s' % str(ads)
         vms = (float(self.inbytesSq) - float(self.inbytes * self.inbytes)/float(self.inmsg))/float(self.inmsg - 1)
         print "std dev inbound message size: " + '%5s' % str(sqrt(vms))
      if self.outmsg > 1:
         ads = float(self.outbytes)/float(self.outmsg)
         print "average outbound message size: " + '%5s' % str(ads)
         vms = (float(self.outbytesSq) - float(self.outbytes * self.outbytes)/float(self.outmsg))/float(self.outmsg - 1)
         print "std dev outbound message size: " + '%5s' % str(sqrt(vms))
      


class filter:
   def __init__(self):
      self.filters = [ ]
  
   def match(self, flow):
      # by default, match everything
      if not self.filters:     
         return True
      # match any filter
      for f in self.filters:
         if f.match(flow):
            return True

   def addFilter(self, f):
      self.filters.append(f)

class conjunctionFilter(filter):

   def match(self, flow):
      # by default, match nothing
      if not self.filters:     
         return False
      # match all filter
      tval = True
      for f in self.filters:
         tval = tval and f.match(flow)
      return tval


class matchType:
   base = 0
   list_any = 1
   list_all = 2

class flowFilter:

   def selectField(self, a):
      if self.field2 is None:
         return a
      else:
         if self.field2 in a:
            return a[self.field2]
         return None

   def __init__(self, string):
      if string is None:
         self.matchAll = True
         return
      else:
         self.matchAll = False

      # remove whitespace
      string = string.replace(" ", "")

      for op in [ '=', '>', '<' ]: 
         if op in string:
            (self.field, self.value) = string.split(op, 2)
      
            # arrays are notated array[all] or array[any]
            if "[all]" in self.field:
               self.field = self.field.replace("[all]", "")
               self.type = matchType.list_all
            elif "[any]" in self.field:
               self.field = self.field.replace("[any]", "")
               self.type = matchType.list_any
            else:
               self.type = matchType.base
            # print self.field

            # subfields are notated "flow.subfield"
            if '.' in self.field:
               (self.field, self.field2) = self.field.split(".", 2)
            else:
               self.field2 = None
         
            if self.value.isdigit():
               self.value = int(self.value)

            if op == '=':
               self.operator = operator.eq 
            if op == '<':
               self.operator = operator.lt 
            if op == '>':
               self.operator = operator.gt 
            # print "filter: " + self.field + " " + str(self.operator) + " " + str(self.value)

   def matchElement(self, filter):
      # print "this is my filter: " + str(filter)
      # print "type: " + str(self.type) + " : " + str(matchType.list_all)
      if self.type is matchType.base:
         if self.operator(self.selectField(filter), self.value):
            return True
         else:
            return False
      elif self.type is matchType.list_all:
         tval = True
         if not filter:
            return False
         for x in filter:
            tval = tval and self.operator(self.selectField(x), self.value)
         return tval
      elif self.type is matchType.list_any:
         if not filter:
            return False
         for x in filter:
            if self.operator(self.selectField(x), self.value):
               return True
         return False

   def match(self, flow):
      if self.matchAll is True:
         return True         
      if self.field in flow:
         return self.matchElement(flow[self.field])


def flowPrint(f):
      print "   {"
      print "      \"flow\": {"
      print "         \"sa\": \"" + str(f["sa"]) + "\","
      print "         \"da\": \"" + str(f["da"]) + "\","
      print "         \"pr\": " + str(f["pr"]) + ","
      print "         \"sp\": " + str(f["sp"]) + ","
      print "         \"dp\": " + str(f["dp"]) + ","
      print "         \"ob\": " + str(f["ob"]) + ","
      print "         \"op\": " + str(f["op"]) + ","
      if "ib" in f:
         print "         \"ib\": " + str(f["ib"]) + ","
         print "         \"ip\": " + str(f["ip"]) + ","         
      print "         \"ts\": " + str(f["ts"]) + ","
      print "         \"te\": " + str(f["te"]) + ","
      print "         \"ottl\": " + str(f["ottl"]) + ","
      if "ittl" in f:
         print "         \"ittl\": " + str(f["ittl"]) + ","
      print "         \"non_norm_stats\": ["
      for x in f["non_norm_stats"][0:-1]:
         print "            ", 
         print "{ \"b\": " + str(x["b"]) + ", \"dir\": \"" + str(x["dir"]) + "\", \"ipt\": " + str(x["ipt"]) + " },"
      if f["non_norm_stats"]:
         x = f["non_norm_stats"][-1]
         print "            ", 
         print "{ \"b\": " + str(x["b"]) + ", \"dir\": \"" + str(x["dir"]) + "\", \"ipt\": " + str(x["ipt"]) + " }"
      print "          ],"

      if "bd" in f:
         print "         \"bd\": ["
         for x in f["bd"][0:-1]:
            print str(x) + ", ", 
         print str(x) 
         print "           ]", 

      # print json.dumps(f, indent=3),
      print "      }\n   }",


class flowProcessor:
   def __init__(self):
      self.firstFlow = 1

   def processFlow(self, flow):
      if not self.firstFlow:
         print ","
      else:
         self.firstFlow = 0
         print "\"appflows\": ["
      flowPrint(flow)

   def processMetadata(self, metadata):
      print "\"metadata\": ", 
      print json.dumps(metadata, indent=3),
      print ","

   def preProcess(self):    
      print "{"

   def postProcess(self):    
      if self.firstFlow:
         self.firstFlow = 0
         print "\"appflows\": ["
      print "]"
      print "}"



class printSelectedElements:
   def __init__(self, field):
      self.firstFlow = 1
      self.field = field
      if '.' in self.field:
         (self.field, self.field2) = self.field.split(".", 2)
         self.depth = 2
      else:
         self.depth = 1

   def processFlow(self, flow):
      # print "   {"
      # print "      \"flow\": ",
      # print json.dumps(flow, indent=3),

      if self.field in flow:
         filter = flow[self.field]
         if self.depth is 1:
            if not self.firstFlow:
               print ","
            else:
               self.firstFlow = 0    
            print  "\t{ \"" + str(self.field) + "\": " + str(filter) + " }", 
         else:
            if type(filter) is list:
               for a in filter:
                  if self.field2 in a:
                     filter2 = a[self.field2]
                     if not self.firstFlow:
                        print ","
                     else:
                        self.firstFlow = 0    
                     print  "\t{ \"" + str(self.field2) + "\": " + str(filter2) + " }", 
            else:
               if self.field2 in filter:
                  filter2 = filter[self.field2]
                  if not self.firstFlow:
                     print ","
                  else:
                     self.firstFlow = 0
                  print  "\t{ \"" + str(self.field2) + "\": " + str(filter2) + " }", 
      # print "   }",

   def processMetadata(self, metadata):
      pass

   def preProcess(self):    
      print "{"
      print "\"" + str(self.field) + "\": ["

   def postProcess(self):    
      print
      print "   ]"
      print "}"

class flowStatsPrinter:
   def __init__(self):
      self.flowdict = {}
      self.flowtotal = flowstats()      

   def processFlow(self, flow):
      #
      # keep separate statistics for each destination port
      dp = flow["dp"]
      if dp not in self.flowdict:
         fs = flowstats()
         self.flowdict[dp] = fs
      else:
         fs = self.flowdict[dp]

      fs.numflows += 1
      self.flowtotal.numflows += 1
      for x in flow['non_norm_stats']:
         fs.observe(x["b"], x["dir"], x["ipt"])
         self.flowtotal.observe(x["b"], x["dir"], x["ipt"])      

   def processMetadata(self, metadata):
      pass

   def preProcess(self):    
      print

   def postProcess(self):      
      # for fs in self.flowdict:
      #   print "flow stats for dp=" + str(fs)
      #   self.flowdict[fs].printflowstats()
      #   print 
      print "total flow stats"
      self.flowtotal.printflowstats()
      # self.flowtotal.print_lengths()
      # self.flowtotal.print_times()


def description(t):
   if t is str:
      return "string"
   if t is int:
      return "int"
   if t is list:
      return "list"
   if t is object:
      return "object"
   if t is float:
      return "float"
   return "unknown"

class printSchema:
   def __init__(self):
      self.firstFlow = 1
      self.indentation = ""

   def processFlow(self, flow):
      for x in flow:
         t = type(flow[x])
         print "type(" + str(flow[x]) + "): " + str(t),
         if t is object:
            print "processing object: " + str(flow[x])
            tmp = self.indentation
            self.indentation = self.indentation + "\t"
            self.processFlow(flow[x])
            self.indentation = tmp
         elif t is list:
            print "processing list: " + str(flow[x])
            tmp = self.indentation
            self.indentation = self.indentation + "\t"
            # self.processFlow((flow[x])[1])
            self.indentation = tmp
         else:
            print self.indentation + "flow." + str(x) + "\t" + description(t)

   def postProcess(self):    
      print

def processFile(f, ff, fp):
   global flowdict, flowtotal
   json_data=open(f)
   data = json.load(json_data)

   if "metadata" in data:
      fp.processMetadata(data["metadata"])

   for flow in data["appflows"]:
      if ff.match(flow["flow"]):
         fp.processFlow(flow["flow"])
   json_data.close()

def usage():
   print
   print "EXAMPLE"
   print "./query.py sample.json --where \" non_norm_stats[any].b = 478 & pr = 6\" --select dp"
   print
   print "FILTER examples:"
   print "  dp = 443"
   print "  dp > 1024"
   print "  sa = 10.0.0.1"
   print "  pr = 17"
   print "  bd[all] > 10"
   print "  bd[any] > 10"
   print "  non_norm_stats[any].b = 41 & ip = 2"
   print "  non_norm_stats[all].ipt < 5 & dp = 80"
   print
   print "SELECTION examples:"
   print "  dp"
   print "  sa"
   print "  ohttp.uri"
   print "  non_norm_stats"
   print "  non_norm_stats.ipt"

#
# main function 
#
if __name__=='__main__':

   parser = OptionParser()
   parser.set_description("filter JSON flow data and print out matching flows, selected fields, or stats")
   parser.add_option("--where", dest="filter", help="filter flows")
   parser.add_option("--select", dest="selection", help="select field to output")
   parser.add_option("--stats", action="store_true", help="print out statistics")
   parser.add_option("--schema", action='store_true', dest="schema", help="print out schema")

   # check args
   if len(sys.argv) < 2:
      parser.print_help()
      usage()
      sys.exit()

   (opts, args) = parser.parse_args()

   if opts.selection is not None:
      fp = printSelectedElements(opts.selection)
   else:
      if opts.schema is True:
         fp = printSchema()
      elif opts.stats is True:
         fp = flowStatsPrinter()
      else:
         fp = flowProcessor()      

   ff = filter()
   if opts.filter:
      for z in opts.filter.split('|'):
         # print "disjunction: " + str(z)
         if '&' in z:
            conjf = conjunctionFilter()
            for conj in z.split('&'):
               # print "conjunction: " + str(conj)
               conjf.addFilter(flowFilter(conj))
            ff.addFilter(conjf)
         else:
            ff.addFilter(flowFilter(z))

   if not args:
      parser.print_help()
      usage()
      sys.exit()

   # process all files, with preamble and postable
   #
   fp.preProcess()
   for x in args:
      processFile(x, ff, fp)
   fp.postProcess()




