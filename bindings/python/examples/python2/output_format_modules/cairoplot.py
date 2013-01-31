#!/usr/bin/env python
# -*- coding: utf-8 -*-

# CairoPlot.py
#
# Copyright (c) 2008 Rodrigo Moreira Araújo
#
# Author: Rodrigo Moreiro Araujo <alf.rodrigo@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

#Contributor: João S. O. Bueno

#TODO: review BarPlot Code
#TODO: x_label colision problem on Horizontal Bar Plot
#TODO: y_label's eat too much space on HBP


__version__ = 1.2

import cairo
import math
import random
from series import Series, Group, Data

HORZ = 0
VERT = 1
NORM = 2

COLORS = {"red"    : (1.0,0.0,0.0,1.0), "lime"    : (0.0,1.0,0.0,1.0), "blue"   : (0.0,0.0,1.0,1.0),
          "maroon" : (0.5,0.0,0.0,1.0), "green"   : (0.0,0.5,0.0,1.0), "navy"   : (0.0,0.0,0.5,1.0),
          "yellow" : (1.0,1.0,0.0,1.0), "magenta" : (1.0,0.0,1.0,1.0), "cyan"   : (0.0,1.0,1.0,1.0),
          "orange" : (1.0,0.5,0.0,1.0), "white"   : (1.0,1.0,1.0,1.0), "black"  : (0.0,0.0,0.0,1.0),
          "gray" : (0.5,0.5,0.5,1.0), "light_gray" : (0.9,0.9,0.9,1.0),
          "transparent" : (0.0,0.0,0.0,0.0)}

THEMES = {"black_red"         : [(0.0,0.0,0.0,1.0), (1.0,0.0,0.0,1.0)],
          "red_green_blue"    : [(1.0,0.0,0.0,1.0), (0.0,1.0,0.0,1.0), (0.0,0.0,1.0,1.0)],
          "red_orange_yellow" : [(1.0,0.2,0.0,1.0), (1.0,0.7,0.0,1.0), (1.0,1.0,0.0,1.0)],
          "yellow_orange_red" : [(1.0,1.0,0.0,1.0), (1.0,0.7,0.0,1.0), (1.0,0.2,0.0,1.0)],
          "rainbow"           : [(1.0,0.0,0.0,1.0), (1.0,0.5,0.0,1.0), (1.0,1.0,0.0,1.0), (0.0,1.0,0.0,1.0), (0.0,0.0,1.0,1.0), (0.3, 0.0, 0.5,1.0), (0.5, 0.0, 1.0, 1.0)]}

def colors_from_theme( theme, series_length, mode = 'solid' ):
    colors = []
    if theme not in THEMES.keys() :
        raise Exception, "Theme not defined"
    color_steps = THEMES[theme]
    n_colors = len(color_steps)
    if series_length <= n_colors:
        colors = [color + tuple([mode]) for color in color_steps[0:n_colors]]
    else:
        iterations = [(series_length - n_colors)/(n_colors - 1) for i in color_steps[:-1]]
        over_iterations = (series_length - n_colors) % (n_colors - 1)
        for i in range(n_colors - 1):
            if over_iterations <= 0:
                break
            iterations[i] += 1
            over_iterations -= 1
        for index,color in enumerate(color_steps[:-1]):
            colors.append(color + tuple([mode]))
            if iterations[index] == 0:
                continue
            next_color = color_steps[index+1]
            color_step = ((next_color[0] - color[0])/(iterations[index] + 1),
                          (next_color[1] - color[1])/(iterations[index] + 1),
                          (next_color[2] - color[2])/(iterations[index] + 1),
                          (next_color[3] - color[3])/(iterations[index] + 1))
            for i in range( iterations[index] ):
                colors.append((color[0] + color_step[0]*(i+1),
                               color[1] + color_step[1]*(i+1),
                               color[2] + color_step[2]*(i+1),
                               color[3] + color_step[3]*(i+1),
                               mode))
        colors.append(color_steps[-1] + tuple([mode]))
    return colors


def other_direction(direction):
    "explicit is better than implicit"
    if direction == HORZ:
        return VERT
    else:
        return HORZ

#Class definition

class Plot(object):
    def __init__(self,
                 surface=None,
                 data=None,
                 width=640,
                 height=480,
                 background=None,
                 border = 0,
                 x_labels = None,
                 y_labels = None,
                 series_colors = None):
        random.seed(2)
        self.create_surface(surface, width, height)
        self.dimensions = {}
        self.dimensions[HORZ] = width
        self.dimensions[VERT] = height
        self.context = cairo.Context(self.surface)
        self.labels={}
        self.labels[HORZ] = x_labels
        self.labels[VERT] = y_labels
        self.load_series(data, x_labels, y_labels, series_colors)
        self.font_size = 10
        self.set_background (background)
        self.border = border
        self.borders = {}
        self.line_color = (0.5, 0.5, 0.5)
        self.line_width = 0.5
        self.label_color = (0.0, 0.0, 0.0)
        self.grid_color = (0.8, 0.8, 0.8)

    def create_surface(self, surface, width=None, height=None):
        self.filename = None
        if isinstance(surface, cairo.Surface):
            self.surface = surface
            return
        if not type(surface) in (str, unicode):
            raise TypeError("Surface should be either a Cairo surface or a filename, not %s" % surface)
        sufix = surface.rsplit(".")[-1].lower()
        self.filename = surface
        if sufix == "png":
            self.surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, width, height)
        elif sufix == "ps":
            self.surface = cairo.PSSurface(surface, width, height)
        elif sufix == "pdf":
            self.surface = cairo.PSSurface(surface, width, height)
        else:
            if sufix != "svg":
                self.filename += ".svg"
            self.surface = cairo.SVGSurface(self.filename, width, height)

    def commit(self):
        try:
            self.context.show_page()
            if self.filename and self.filename.endswith(".png"):
                self.surface.write_to_png(self.filename)
            else:
                self.surface.finish()
        except cairo.Error:
            pass

    def load_series (self, data, x_labels=None, y_labels=None, series_colors=None):
        self.series_labels = []
        self.series = None

        #The pretty way
        #if not isinstance(data, Series):
        #    # Not an instance of Series
        #    self.series = Series(data)
        #else:
        #    self.series = data
        #
        #self.series_labels = self.series.get_names()

        #TODO: Remove on next version
        # The ugly way, keeping retrocompatibility...
        if callable(data) or type(data) is list and callable(data[0]): # Lambda or List of lambdas
            self.series = data
            self.series_labels = None
        elif isinstance(data, Series): # Instance of Series
            self.series = data
            self.series_labels = data.get_names()
        else: # Anything else
            self.series = Series(data)
            self.series_labels = self.series.get_names()

        #TODO: allow user passed series_widths
        self.series_widths = [1.0 for group in self.series]

        #TODO: Remove on next version
        self.process_colors( series_colors )

    def process_colors( self, series_colors, length = None, mode = 'solid' ):
        #series_colors might be None, a theme, a string of colors names or a list of color tuples
        if length is None :
            length = len( self.series.to_list() )

        #no colors passed
        if not series_colors:
            #Randomize colors
            self.series_colors = [ [random.random() for i in range(3)] + [1.0, mode]  for series in range( length ) ]
        else:
            #Just theme pattern
            if not hasattr( series_colors, "__iter__" ):
                theme = series_colors
                self.series_colors = colors_from_theme( theme.lower(), length )

            #Theme pattern and mode
            elif not hasattr(series_colors, '__delitem__') and not hasattr( series_colors[0], "__iter__" ):
                theme = series_colors[0]
                mode = series_colors[1]
                self.series_colors = colors_from_theme( theme.lower(), length, mode )

            #List
            else:
                self.series_colors = series_colors
                for index, color in enumerate( self.series_colors ):
                    #element is a color name
                    if not hasattr(color, "__iter__"):
                        self.series_colors[index] = COLORS[color.lower()] + tuple([mode])
                    #element is rgb tuple instead of rgba
                    elif len( color ) == 3 :
                        self.series_colors[index] += (1.0,mode)
                    #element has 4 elements, might be rgba tuple or rgb tuple with mode
                    elif len( color ) == 4 :
                        #last element is mode
                        if not hasattr(color[3], "__iter__"):
                            self.series_colors[index] += tuple([color[3]])
                            self.series_colors[index][3] = 1.0
                        #last element is alpha
                        else:
                            self.series_colors[index] += tuple([mode])

    def get_width(self):
        return self.surface.get_width()

    def get_height(self):
        return self.surface.get_height()

    def set_background(self, background):
        if background is None:
            self.background = (0.0,0.0,0.0,0.0)
        elif type(background) in (cairo.LinearGradient, tuple):
            self.background = background
        elif not hasattr(background,"__iter__"):
            colors = background.split(" ")
            if len(colors) == 1 and colors[0] in COLORS:
                self.background = COLORS[background]
            elif len(colors) > 1:
                self.background = cairo.LinearGradient(self.dimensions[HORZ] / 2, 0, self.dimensions[HORZ] / 2, self.dimensions[VERT])
                for index,color in enumerate(colors):
                    self.background.add_color_stop_rgba(float(index)/(len(colors)-1),*COLORS[color])
        else:
            raise TypeError ("Background should be either cairo.LinearGradient or a 3/4-tuple, not %s" % type(background))

    def render_background(self):
        if isinstance(self.background, cairo.LinearGradient):
            self.context.set_source(self.background)
        else:
            self.context.set_source_rgba(*self.background)
        self.context.rectangle(0,0, self.dimensions[HORZ], self.dimensions[VERT])
        self.context.fill()

    def render_bounding_box(self):
        self.context.set_source_rgba(*self.line_color)
        self.context.set_line_width(self.line_width)
        self.context.rectangle(self.border, self.border,
                               self.dimensions[HORZ] - 2 * self.border,
                               self.dimensions[VERT] - 2 * self.border)
        self.context.stroke()

    def render(self):
        pass

class ScatterPlot( Plot ):
    def __init__(self,
                 surface=None,
                 data=None,
                 errorx=None,
                 errory=None,
                 width=640,
                 height=480,
                 background=None,
                 border=0,
                 axis = False,
                 dash = False,
                 discrete = False,
                 dots = 0,
                 grid = False,
                 series_legend = False,
                 x_labels = None,
                 y_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 z_bounds = None,
                 x_title  = None,
                 y_title  = None,
                 series_colors = None,
                 circle_colors = None ):

        self.bounds = {}
        self.bounds[HORZ] = x_bounds
        self.bounds[VERT] = y_bounds
        self.bounds[NORM] = z_bounds
        self.titles = {}
        self.titles[HORZ] = x_title
        self.titles[VERT] = y_title
        self.max_value = {}
        self.axis = axis
        self.discrete = discrete
        self.dots = dots
        self.grid = grid
        self.series_legend = series_legend
        self.variable_radius = False
        self.x_label_angle = math.pi / 2.5
        self.circle_colors = circle_colors

        Plot.__init__(self, surface, data, width, height, background, border, x_labels, y_labels, series_colors)

        self.dash = None
        if dash:
            if hasattr(dash, "keys"):
                self.dash = [dash[key] for key in self.series_labels]
            elif max([hasattr(item,'__delitem__') for item in data]) :
                self.dash = dash
            else:
                self.dash = [dash]

        self.load_errors(errorx, errory)

    def convert_list_to_tuple(self, data):
        #Data must be converted from lists of coordinates to a single
        # list of tuples
        out_data = zip(*data)
        if len(data) == 3:
            self.variable_radius = True
        return out_data

    def load_series(self, data, x_labels = None, y_labels = None, series_colors=None):
        #TODO: In cairoplot 2.0 keep only the Series instances

        # Convert Data and Group to Series
        if isinstance(data, Data) or isinstance(data, Group):
            data = Series(data)

        # Series
        if  isinstance(data, Series):
            for group in data:
                for item in group:
                    if len(item) is 3:
                        self.variable_radius = True

        #Dictionary with lists
        if hasattr(data, "keys") :
            if hasattr( data.values()[0][0], "__delitem__" ) :
                for key in data.keys() :
                    data[key] = self.convert_list_to_tuple(data[key])
            elif len(data.values()[0][0]) == 3:
                    self.variable_radius = True
        #List
        elif hasattr(data[0], "__delitem__") :
            #List of lists
            if hasattr(data[0][0], "__delitem__") :
                for index,value in enumerate(data) :
                    data[index] = self.convert_list_to_tuple(value)
            #List
            elif type(data[0][0]) != type((0,0)):
                data = self.convert_list_to_tuple(data)
            #Three dimensional data
            elif len(data[0][0]) == 3:
                self.variable_radius = True

        #List with three dimensional tuples
        elif len(data[0]) == 3:
            self.variable_radius = True
        Plot.load_series(self, data, x_labels, y_labels, series_colors)
        self.calc_boundaries()
        self.calc_labels()

    def load_errors(self, errorx, errory):
        self.errors = None
        if errorx == None and errory == None:
            return
        self.errors = {}
        self.errors[HORZ] = None
        self.errors[VERT] = None
        #asimetric errors
        if errorx and hasattr(errorx[0], "__delitem__"):
            self.errors[HORZ] = errorx
        #simetric errors
        elif errorx:
            self.errors[HORZ] = [errorx]
        #asimetric errors
        if errory and hasattr(errory[0], "__delitem__"):
            self.errors[VERT] = errory
        #simetric errors
        elif errory:
            self.errors[VERT] = [errory]

    def calc_labels(self):
        if not self.labels[HORZ]:
            amplitude = self.bounds[HORZ][1] - self.bounds[HORZ][0]
            if amplitude % 10: #if horizontal labels need floating points
                self.labels[HORZ] = ["%.2lf" % (float(self.bounds[HORZ][0] + (amplitude * i / 10.0))) for i in range(11) ]
            else:
                self.labels[HORZ] = ["%d" % (int(self.bounds[HORZ][0] + (amplitude * i / 10.0))) for i in range(11) ]
        if not self.labels[VERT]:
            amplitude = self.bounds[VERT][1] - self.bounds[VERT][0]
            if amplitude % 10: #if vertical labels need floating points
                self.labels[VERT] = ["%.2lf" % (float(self.bounds[VERT][0] + (amplitude * i / 10.0))) for i in range(11) ]
            else:
                self.labels[VERT] = ["%d" % (int(self.bounds[VERT][0] + (amplitude * i / 10.0))) for i in range(11) ]

    def calc_extents(self, direction):
        self.context.set_font_size(self.font_size * 0.8)
        self.max_value[direction] = max(self.context.text_extents(item)[2] for item in self.labels[direction])
        self.borders[other_direction(direction)] = self.max_value[direction] + self.border + 20

    def calc_boundaries(self):
        #HORZ = 0, VERT = 1, NORM = 2
        min_data_value = [0,0,0]
        max_data_value = [0,0,0]

        for group in self.series:
            if type(group[0].content) in (int, float, long):
                group = [Data((index, item.content)) for index,item in enumerate(group)]

            for point in group:
                for index, item in enumerate(point.content):
                    if item > max_data_value[index]:
                        max_data_value[index] = item
                    elif item < min_data_value[index]:
                        min_data_value[index] = item

        if not self.bounds[HORZ]:
            self.bounds[HORZ] = (min_data_value[HORZ], max_data_value[HORZ])
        if not self.bounds[VERT]:
            self.bounds[VERT] = (min_data_value[VERT], max_data_value[VERT])
        if not self.bounds[NORM]:
            self.bounds[NORM] = (min_data_value[NORM], max_data_value[NORM])

    def calc_all_extents(self):
        self.calc_extents(HORZ)
        self.calc_extents(VERT)

        self.plot_height = self.dimensions[VERT] - 2 * self.borders[VERT]
        self.plot_width = self.dimensions[HORZ] - 2* self.borders[HORZ]

        self.plot_top = self.dimensions[VERT] - self.borders[VERT]

    def calc_steps(self):
        #Calculates all the x, y, z and color steps
        series_amplitude = [self.bounds[index][1] - self.bounds[index][0] for index in range(3)]

        if series_amplitude[HORZ]:
            self.horizontal_step = float (self.plot_width) / series_amplitude[HORZ]
        else:
            self.horizontal_step = 0.00

        if series_amplitude[VERT]:
            self.vertical_step = float (self.plot_height) / series_amplitude[VERT]
        else:
            self.vertical_step = 0.00

        if series_amplitude[NORM]:
            if self.variable_radius:
                self.z_step = float (self.bounds[NORM][1]) / series_amplitude[NORM]
            if self.circle_colors:
                self.circle_color_step = tuple([float(self.circle_colors[1][i]-self.circle_colors[0][i])/series_amplitude[NORM] for i in range(4)])
        else:
            self.z_step = 0.00
            self.circle_color_step = ( 0.0, 0.0, 0.0, 0.0 )

    def get_circle_color(self, value):
        return tuple( [self.circle_colors[0][i] + value*self.circle_color_step[i] for i in range(4)] )

    def render(self):
        self.calc_all_extents()
        self.calc_steps()
        self.render_background()
        self.render_bounding_box()
        if self.axis:
            self.render_axis()
        if self.grid:
            self.render_grid()
        self.render_labels()
        self.render_plot()
        if self.errors:
            self.render_errors()
        if self.series_legend and self.series_labels:
            self.render_legend()

    def render_axis(self):
        #Draws both the axis lines and their titles
        cr = self.context
        cr.set_source_rgba(*self.line_color)
        cr.move_to(self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT])
        cr.line_to(self.borders[HORZ], self.borders[VERT])
        cr.stroke()

        cr.move_to(self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT])
        cr.line_to(self.dimensions[HORZ] - self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT])
        cr.stroke()

        cr.set_source_rgba(*self.label_color)
        self.context.set_font_size( 1.2 * self.font_size )
        if self.titles[HORZ]:
            title_width,title_height = cr.text_extents(self.titles[HORZ])[2:4]
            cr.move_to( self.dimensions[HORZ]/2 - title_width/2, self.borders[VERT] - title_height/2 )
            cr.show_text( self.titles[HORZ] )

        if self.titles[VERT]:
            title_width,title_height = cr.text_extents(self.titles[VERT])[2:4]
            cr.move_to( self.dimensions[HORZ] - self.borders[HORZ] + title_height/2, self.dimensions[VERT]/2 - title_width/2)
            cr.save()
            cr.rotate( math.pi/2 )
            cr.show_text( self.titles[VERT] )
            cr.restore()

    def render_grid(self):
        cr = self.context
        horizontal_step = float( self.plot_height ) / ( len( self.labels[VERT] ) - 1 )
        vertical_step = float( self.plot_width ) / ( len( self.labels[HORZ] ) - 1 )

        x = self.borders[HORZ] + vertical_step
        y = self.plot_top - horizontal_step

        for label in self.labels[HORZ][:-1]:
            cr.set_source_rgba(*self.grid_color)
            cr.move_to(x, self.dimensions[VERT] - self.borders[VERT])
            cr.line_to(x, self.borders[VERT])
            cr.stroke()
            x += vertical_step
        for label in self.labels[VERT][:-1]:
            cr.set_source_rgba(*self.grid_color)
            cr.move_to(self.borders[HORZ], y)
            cr.line_to(self.dimensions[HORZ] - self.borders[HORZ], y)
            cr.stroke()
            y -= horizontal_step

    def render_labels(self):
        self.context.set_font_size(self.font_size * 0.8)
        self.render_horz_labels()
        self.render_vert_labels()

    def render_horz_labels(self):
        cr = self.context
        step = float( self.plot_width ) / ( len( self.labels[HORZ] ) - 1 )
        x = self.borders[HORZ]
        y = self.dimensions[VERT] - self.borders[VERT] + 5

        # store rotation matrix from the initial state
        rotation_matrix = cr.get_matrix()
        rotation_matrix.rotate(self.x_label_angle)

        cr.set_source_rgba(*self.label_color)

        for item in self.labels[HORZ]:
            width = cr.text_extents(item)[2]
            cr.move_to(x, y)
            cr.save()
            cr.set_matrix(rotation_matrix)
            cr.show_text(item)
            cr.restore()
            x += step

    def render_vert_labels(self):
        cr = self.context
        step = ( self.plot_height ) / ( len( self.labels[VERT] ) - 1 )
        y = self.plot_top
        cr.set_source_rgba(*self.label_color)
        for item in self.labels[VERT]:
            width = cr.text_extents(item)[2]
            cr.move_to(self.borders[HORZ] - width - 5,y)
            cr.show_text(item)
            y -= step

    def render_legend(self):
        cr = self.context
        cr.set_font_size(self.font_size)
        cr.set_line_width(self.line_width)

        widest_word = max(self.series_labels, key = lambda item: self.context.text_extents(item)[2])
        tallest_word = max(self.series_labels, key = lambda item: self.context.text_extents(item)[3])
        max_width = self.context.text_extents(widest_word)[2]
        max_height = self.context.text_extents(tallest_word)[3] * 1.1

        color_box_height = max_height / 2
        color_box_width = color_box_height * 2

        #Draw a bounding box
        bounding_box_width = max_width + color_box_width + 15
        bounding_box_height = (len(self.series_labels)+0.5) * max_height
        cr.set_source_rgba(1,1,1)
        cr.rectangle(self.dimensions[HORZ] - self.borders[HORZ] - bounding_box_width, self.borders[VERT],
                            bounding_box_width, bounding_box_height)
        cr.fill()

        cr.set_source_rgba(*self.line_color)
        cr.set_line_width(self.line_width)
        cr.rectangle(self.dimensions[HORZ] - self.borders[HORZ] - bounding_box_width, self.borders[VERT],
                            bounding_box_width, bounding_box_height)
        cr.stroke()

        for idx,key in enumerate(self.series_labels):
            #Draw color box
            cr.set_source_rgba(*self.series_colors[idx][:4])
            cr.rectangle(self.dimensions[HORZ] - self.borders[HORZ] - max_width - color_box_width - 10,
                                self.borders[VERT] + color_box_height + (idx*max_height) ,
                                color_box_width, color_box_height)
            cr.fill()

            cr.set_source_rgba(0, 0, 0)
            cr.rectangle(self.dimensions[HORZ] - self.borders[HORZ] - max_width - color_box_width - 10,
                                self.borders[VERT] + color_box_height + (idx*max_height),
                                color_box_width, color_box_height)
            cr.stroke()

            #Draw series labels
            cr.set_source_rgba(0, 0, 0)
            cr.move_to(self.dimensions[HORZ] - self.borders[HORZ] - max_width - 5, self.borders[VERT] + ((idx+1)*max_height))
            cr.show_text(key)

    def render_errors(self):
        cr = self.context
        cr.rectangle(self.borders[HORZ], self.borders[VERT], self.plot_width, self.plot_height)
        cr.clip()
        radius = self.dots
        x0 = self.borders[HORZ] - self.bounds[HORZ][0]*self.horizontal_step
        y0 = self.borders[VERT] - self.bounds[VERT][0]*self.vertical_step
        for index, group in enumerate(self.series):
            cr.set_source_rgba(*self.series_colors[index][:4])
            for number, data in enumerate(group):
                x = x0 + self.horizontal_step * data.content[0]
                y = self.dimensions[VERT] - y0 - self.vertical_step * data.content[1]
                if self.errors[HORZ]:
                    cr.move_to(x, y)
                    x1 = x - self.horizontal_step * self.errors[HORZ][0][number]
                    cr.line_to(x1, y)
                    cr.line_to(x1, y - radius)
                    cr.line_to(x1, y + radius)
                    cr.stroke()
                if self.errors[HORZ] and len(self.errors[HORZ]) == 2:
                    cr.move_to(x, y)
                    x1 = x + self.horizontal_step * self.errors[HORZ][1][number]
                    cr.line_to(x1, y)
                    cr.line_to(x1, y - radius)
                    cr.line_to(x1, y + radius)
                    cr.stroke()
                if self.errors[VERT]:
                    cr.move_to(x, y)
                    y1 = y + self.vertical_step   * self.errors[VERT][0][number]
                    cr.line_to(x, y1)
                    cr.line_to(x - radius, y1)
                    cr.line_to(x + radius, y1)
                    cr.stroke()
                if self.errors[VERT] and len(self.errors[VERT]) == 2:
                    cr.move_to(x, y)
                    y1 = y - self.vertical_step   * self.errors[VERT][1][number]
                    cr.line_to(x, y1)
                    cr.line_to(x - radius, y1)
                    cr.line_to(x + radius, y1)
                    cr.stroke()


    def render_plot(self):
        cr = self.context
        if self.discrete:
            cr.rectangle(self.borders[HORZ], self.borders[VERT], self.plot_width, self.plot_height)
            cr.clip()
            x0 = self.borders[HORZ] - self.bounds[HORZ][0]*self.horizontal_step
            y0 = self.borders[VERT] - self.bounds[VERT][0]*self.vertical_step
            radius = self.dots
            for number, group in  enumerate (self.series):
                cr.set_source_rgba(*self.series_colors[number][:4])
                for data in group :
                    if self.variable_radius:
                        radius = data.content[2]*self.z_step
                        if self.circle_colors:
                            cr.set_source_rgba( *self.get_circle_color( data.content[2]) )
                    x = x0 + self.horizontal_step*data.content[0]
                    y = y0 + self.vertical_step*data.content[1]
                    cr.arc(x, self.dimensions[VERT] - y, radius, 0, 2*math.pi)
                    cr.fill()
        else:
            cr.rectangle(self.borders[HORZ], self.borders[VERT], self.plot_width, self.plot_height)
            cr.clip()
            x0 = self.borders[HORZ] - self.bounds[HORZ][0]*self.horizontal_step
            y0 = self.borders[VERT] - self.bounds[VERT][0]*self.vertical_step
            radius = self.dots
            for number, group in  enumerate (self.series):
                last_data = None
                cr.set_source_rgba(*self.series_colors[number][:4])
                for data in group :
                    x = x0 + self.horizontal_step*data.content[0]
                    y = y0 + self.vertical_step*data.content[1]
                    if self.dots:
                        if self.variable_radius:
                            radius = data.content[2]*self.z_step
                        cr.arc(x, self.dimensions[VERT] - y, radius, 0, 2*math.pi)
                        cr.fill()
                    if last_data :
                        old_x = x0 + self.horizontal_step*last_data.content[0]
                        old_y = y0 + self.vertical_step*last_data.content[1]
                        cr.move_to( old_x, self.dimensions[VERT] - old_y )
                        cr.line_to( x, self.dimensions[VERT] - y)
                        cr.set_line_width(self.series_widths[number])

                        # Display line as dash line
                        if self.dash and self.dash[number]:
                            s = self.series_widths[number]
                            cr.set_dash([s*3, s*3], 0)

                        cr.stroke()
                        cr.set_dash([])
                    last_data = data

class DotLinePlot(ScatterPlot):
    def __init__(self,
                 surface=None,
                 data=None,
                 width=640,
                 height=480,
                 background=None,
                 border=0,
                 axis = False,
                 dash = False,
                 dots = 0,
                 grid = False,
                 series_legend = False,
                 x_labels = None,
                 y_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 x_title  = None,
                 y_title  = None,
                 series_colors = None):

        ScatterPlot.__init__(self, surface, data, None, None, width, height, background, border,
                             axis, dash, False, dots, grid, series_legend, x_labels, y_labels,
                             x_bounds, y_bounds, None, x_title, y_title, series_colors, None )


    def load_series(self, data, x_labels = None, y_labels = None, series_colors=None):
        Plot.load_series(self, data, x_labels, y_labels, series_colors)
        for group in self.series :
            for index,data in enumerate(group):
                group[index].content = (index, data.content)

        self.calc_boundaries()
        self.calc_labels()

class FunctionPlot(ScatterPlot):
    def __init__(self,
                 surface=None,
                 data=None,
                 width=640,
                 height=480,
                 background=None,
                 border=0,
                 axis = False,
                 discrete = False,
                 dots = 0,
                 grid = False,
                 series_legend = False,
                 x_labels = None,
                 y_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 x_title  = None,
                 y_title  = None,
                 series_colors = None,
                 step = 1):

        self.function = data
        self.step = step
        self.discrete = discrete

        data, x_bounds = self.load_series_from_function( self.function, x_bounds )

        ScatterPlot.__init__(self, surface, data, None, None, width, height, background, border,
                             axis, False, discrete, dots, grid, series_legend, x_labels, y_labels,
                             x_bounds, y_bounds, None, x_title, y_title, series_colors, None )

    def load_series(self, data, x_labels = None, y_labels = None, series_colors=None):
        Plot.load_series(self, data, x_labels, y_labels, series_colors)

        if len(self.series[0][0]) is 1:
            for group_id, group in enumerate(self.series) :
                for index,data in enumerate(group):
                    group[index].content = (self.bounds[HORZ][0] + self.step*index, data.content)

        self.calc_boundaries()
        self.calc_labels()

    def load_series_from_function( self, function, x_bounds ):
        #TODO: Add the possibility for the user to define multiple functions with different discretization parameters

        #This function converts a function, a list of functions or a dictionary
        #of functions into its corresponding array of data
        series = Series()

        if isinstance(function, Group) or isinstance(function, Data):
            function = Series(function)

        # If is instance of Series
        if isinstance(function, Series):
            # Overwrite any bounds passed by the function
            x_bounds = (function.range[0],function.range[-1])

        #if no bounds are provided
        if x_bounds == None:
            x_bounds = (0,10)


        #TODO: Finish the dict translation
        if hasattr(function, "keys"): #dictionary:
            for key in function.keys():
                group = Group(name=key)
                #data[ key ] = []
                i = x_bounds[0]
                while i <= x_bounds[1] :
                    group.add_data(function[ key ](i))
                    #data[ key ].append( function[ key ](i) )
                    i += self.step
                series.add_group(group)

        elif hasattr(function, "__delitem__"): #list of functions
            for index,f in enumerate( function ) :
                group = Group()
                #data.append( [] )
                i = x_bounds[0]
                while i <= x_bounds[1] :
                    group.add_data(f(i))
                    #data[ index ].append( f(i) )
                    i += self.step
                series.add_group(group)

        elif isinstance(function, Series): # instance of Series
            series = function

        else: #function
            group = Group()
            i = x_bounds[0]
            while i <= x_bounds[1] :
                group.add_data(function(i))
                i += self.step
            series.add_group(group)


        return series, x_bounds

    def calc_labels(self):
        if not self.labels[HORZ]:
            self.labels[HORZ] = []
            i = self.bounds[HORZ][0]
            while i<=self.bounds[HORZ][1]:
                self.labels[HORZ].append(str(i))
                i += float(self.bounds[HORZ][1] - self.bounds[HORZ][0])/10
        ScatterPlot.calc_labels(self)

    def render_plot(self):
        if not self.discrete:
            ScatterPlot.render_plot(self)
        else:
            last = None
            cr = self.context
            for number, group in  enumerate (self.series):
                cr.set_source_rgba(*self.series_colors[number][:4])
                x0 = self.borders[HORZ] - self.bounds[HORZ][0]*self.horizontal_step
                y0 = self.borders[VERT] - self.bounds[VERT][0]*self.vertical_step
                for data in group:
                    x = x0 + self.horizontal_step * data.content[0]
                    y = y0 + self.vertical_step   * data.content[1]
                    cr.move_to(x, self.dimensions[VERT] - y)
                    cr.line_to(x, self.plot_top)
                    cr.set_line_width(self.series_widths[number])
                    cr.stroke()
                    if self.dots:
                        cr.new_path()
                        cr.arc(x, self.dimensions[VERT] - y, 3, 0, 2.1 * math.pi)
                        cr.close_path()
                        cr.fill()

class BarPlot(Plot):
    def __init__(self,
                 surface = None,
                 data = None,
                 width = 640,
                 height = 480,
                 background = "white light_gray",
                 border = 0,
                 display_values = False,
                 grid = False,
                 rounded_corners = False,
                 stack = False,
                 three_dimension = False,
                 x_labels = None,
                 y_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 series_colors = None,
                 main_dir = None):

        self.bounds = {}
        self.bounds[HORZ] = x_bounds
        self.bounds[VERT] = y_bounds
        self.display_values = display_values
        self.grid = grid
        self.rounded_corners = rounded_corners
        self.stack = stack
        self.three_dimension = three_dimension
        self.x_label_angle = math.pi / 2.5
        self.main_dir = main_dir
        self.max_value = {}
        self.plot_dimensions = {}
        self.steps = {}
        self.value_label_color = (0.5,0.5,0.5,1.0)

        Plot.__init__(self, surface, data, width, height, background, border, x_labels, y_labels, series_colors)

    def load_series(self, data, x_labels = None, y_labels = None, series_colors = None):
        Plot.load_series(self, data, x_labels, y_labels, series_colors)
        self.calc_boundaries()

    def process_colors(self, series_colors):
        #Data for a BarPlot might be a List or a List of Lists.
        #On the first case, colors must be generated for all bars,
        #On the second, colors must be generated for each of the inner lists.

        #TODO: Didn't get it...
        #if hasattr(self.data[0], '__getitem__'):
        #    length = max(len(series) for series in self.data)
        #else:
        #    length = len( self.data )

        length = max(len(group) for group in self.series)

        Plot.process_colors( self, series_colors, length, 'linear')

    def calc_boundaries(self):
        if not self.bounds[self.main_dir]:
            if self.stack:
                max_data_value = max(sum(group.to_list()) for group in self.series)
            else:
                max_data_value = max(max(group.to_list()) for group in self.series)
            self.bounds[self.main_dir] = (0, max_data_value)
        if not self.bounds[other_direction(self.main_dir)]:
            self.bounds[other_direction(self.main_dir)] = (0, len(self.series))

    def calc_extents(self, direction):
        self.max_value[direction] = 0
        if self.labels[direction]:
            widest_word = max(self.labels[direction], key = lambda item: self.context.text_extents(item)[2])
            self.max_value[direction] = self.context.text_extents(widest_word)[3 - direction]
            self.borders[other_direction(direction)] = (2-direction)*self.max_value[direction] + self.border + direction*(5)
        else:
            self.borders[other_direction(direction)] = self.border

    def calc_horz_extents(self):
        self.calc_extents(HORZ)

    def calc_vert_extents(self):
        self.calc_extents(VERT)

    def calc_all_extents(self):
        self.calc_horz_extents()
        self.calc_vert_extents()
        other_dir = other_direction(self.main_dir)
        self.value_label = 0
        if self.display_values:
            if self.stack:
                self.value_label = self.context.text_extents(str(max(sum(group.to_list()) for group in self.series)))[2 + self.main_dir]
            else:
                self.value_label = self.context.text_extents(str(max(max(group.to_list()) for group in self.series)))[2 + self.main_dir]
        if self.labels[self.main_dir]:
            self.plot_dimensions[self.main_dir] = self.dimensions[self.main_dir] - 2*self.borders[self.main_dir] - self.value_label
        else:
            self.plot_dimensions[self.main_dir] = self.dimensions[self.main_dir] - self.borders[self.main_dir] - 1.2*self.border - self.value_label
        self.plot_dimensions[other_dir] = self.dimensions[other_dir] - self.borders[other_dir] - self.border
        self.plot_top = self.dimensions[VERT] - self.borders[VERT]

    def calc_steps(self):
        other_dir = other_direction(self.main_dir)
        self.series_amplitude = self.bounds[self.main_dir][1] - self.bounds[self.main_dir][0]
        if self.series_amplitude:
            self.steps[self.main_dir] = float(self.plot_dimensions[self.main_dir])/self.series_amplitude
        else:
            self.steps[self.main_dir] = 0.00
        series_length = len(self.series)
        self.steps[other_dir] = float(self.plot_dimensions[other_dir])/(series_length + 0.1*(series_length + 1))
        self.space = 0.1*self.steps[other_dir]

    def render(self):
        self.calc_all_extents()
        self.calc_steps()
        self.render_background()
        self.render_bounding_box()
        if self.grid:
            self.render_grid()
        if self.three_dimension:
            self.render_ground()
        if self.display_values:
            self.render_values()
        self.render_labels()
        self.render_plot()
        if self.series_labels:
            self.render_legend()

    def draw_3d_rectangle_front(self, x0, y0, x1, y1, shift):
        self.context.rectangle(x0-shift, y0+shift, x1-x0, y1-y0)

    def draw_3d_rectangle_side(self, x0, y0, x1, y1, shift):
        self.context.move_to(x1-shift,y0+shift)
        self.context.line_to(x1, y0)
        self.context.line_to(x1, y1)
        self.context.line_to(x1-shift, y1+shift)
        self.context.line_to(x1-shift, y0+shift)
        self.context.close_path()

    def draw_3d_rectangle_top(self, x0, y0, x1, y1, shift):
        self.context.move_to(x0-shift,y0+shift)
        self.context.line_to(x0, y0)
        self.context.line_to(x1, y0)
        self.context.line_to(x1-shift, y0+shift)
        self.context.line_to(x0-shift, y0+shift)
        self.context.close_path()

    def draw_round_rectangle(self, x0, y0, x1, y1):
        self.context.arc(x0+5, y0+5, 5, -math.pi, -math.pi/2)
        self.context.line_to(x1-5, y0)
        self.context.arc(x1-5, y0+5, 5, -math.pi/2, 0)
        self.context.line_to(x1, y1-5)
        self.context.arc(x1-5, y1-5, 5, 0, math.pi/2)
        self.context.line_to(x0+5, y1)
        self.context.arc(x0+5, y1-5, 5, math.pi/2, math.pi)
        self.context.line_to(x0, y0+5)
        self.context.close_path()

    def render_ground(self):
        self.draw_3d_rectangle_front(self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT],
                                     self.dimensions[HORZ] - self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT] + 5, 10)
        self.context.fill()

        self.draw_3d_rectangle_side (self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT],
                                     self.dimensions[HORZ] - self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT] + 5, 10)
        self.context.fill()

        self.draw_3d_rectangle_top  (self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT],
                                     self.dimensions[HORZ] - self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT] + 5, 10)
        self.context.fill()

    def render_labels(self):
        self.context.set_font_size(self.font_size * 0.8)
        if self.labels[HORZ]:
            self.render_horz_labels()
        if self.labels[VERT]:
            self.render_vert_labels()

    def render_legend(self):
        cr = self.context
        cr.set_font_size(self.font_size)
        cr.set_line_width(self.line_width)

        widest_word = max(self.series_labels, key = lambda item: self.context.text_extents(item)[2])
        tallest_word = max(self.series_labels, key = lambda item: self.context.text_extents(item)[3])
        max_width = self.context.text_extents(widest_word)[2]
        max_height = self.context.text_extents(tallest_word)[3] * 1.1 + 5

        color_box_height = max_height / 2
        color_box_width = color_box_height * 2

        #Draw a bounding box
        bounding_box_width = max_width + color_box_width + 15
        bounding_box_height = (len(self.series_labels)+0.5) * max_height
        cr.set_source_rgba(1,1,1)
        cr.rectangle(self.dimensions[HORZ] - self.border - bounding_box_width, self.border,
                            bounding_box_width, bounding_box_height)
        cr.fill()

        cr.set_source_rgba(*self.line_color)
        cr.set_line_width(self.line_width)
        cr.rectangle(self.dimensions[HORZ] - self.border - bounding_box_width, self.border,
                            bounding_box_width, bounding_box_height)
        cr.stroke()

        for idx,key in enumerate(self.series_labels):
            #Draw color box
            cr.set_source_rgba(*self.series_colors[idx][:4])
            cr.rectangle(self.dimensions[HORZ] - self.border - max_width - color_box_width - 10,
                                self.border + color_box_height + (idx*max_height) ,
                                color_box_width, color_box_height)
            cr.fill()

            cr.set_source_rgba(0, 0, 0)
            cr.rectangle(self.dimensions[HORZ] - self.border - max_width - color_box_width - 10,
                                self.border + color_box_height + (idx*max_height),
                                color_box_width, color_box_height)
            cr.stroke()

            #Draw series labels
            cr.set_source_rgba(0, 0, 0)
            cr.move_to(self.dimensions[HORZ] - self.border - max_width - 5, self.border + ((idx+1)*max_height))
            cr.show_text(key)


class HorizontalBarPlot(BarPlot):
    def __init__(self,
                 surface = None,
                 data = None,
                 width = 640,
                 height = 480,
                 background = "white light_gray",
                 border = 0,
                 display_values = False,
                 grid = False,
                 rounded_corners = False,
                 stack = False,
                 three_dimension = False,
                 series_labels = None,
                 x_labels = None,
                 y_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 series_colors = None):

        BarPlot.__init__(self, surface, data, width, height, background, border,
                         display_values, grid, rounded_corners, stack, three_dimension,
                         x_labels, y_labels, x_bounds, y_bounds, series_colors, HORZ)
        self.series_labels = series_labels

    def calc_vert_extents(self):
        self.calc_extents(VERT)
        if self.labels[HORZ] and not self.labels[VERT]:
            self.borders[HORZ] += 10

    def draw_rectangle_bottom(self, x0, y0, x1, y1):
        self.context.arc(x0+5, y1-5, 5, math.pi/2, math.pi)
        self.context.line_to(x0, y0+5)
        self.context.arc(x0+5, y0+5, 5, -math.pi, -math.pi/2)
        self.context.line_to(x1, y0)
        self.context.line_to(x1, y1)
        self.context.line_to(x0+5, y1)
        self.context.close_path()

    def draw_rectangle_top(self, x0, y0, x1, y1):
        self.context.arc(x1-5, y0+5, 5, -math.pi/2, 0)
        self.context.line_to(x1, y1-5)
        self.context.arc(x1-5, y1-5, 5, 0, math.pi/2)
        self.context.line_to(x0, y1)
        self.context.line_to(x0, y0)
        self.context.line_to(x1, y0)
        self.context.close_path()

    def draw_rectangle(self, index, length, x0, y0, x1, y1):
        if length == 1:
            BarPlot.draw_rectangle(self, x0, y0, x1, y1)
        elif index == 0:
            self.draw_rectangle_bottom(x0, y0, x1, y1)
        elif index == length-1:
            self.draw_rectangle_top(x0, y0, x1, y1)
        else:
            self.context.rectangle(x0, y0, x1-x0, y1-y0)

    #TODO: Review BarPlot.render_grid code
    def render_grid(self):
        self.context.set_source_rgba(0.8, 0.8, 0.8)
        if self.labels[HORZ]:
            self.context.set_font_size(self.font_size * 0.8)
            step = (self.dimensions[HORZ] - 2*self.borders[HORZ] - self.value_label)/(len(self.labels[HORZ])-1)
            x = self.borders[HORZ]
            next_x = 0
            for item in self.labels[HORZ]:
                width = self.context.text_extents(item)[2]
                if x - width/2 > next_x and x - width/2 > self.border:
                    self.context.move_to(x, self.border)
                    self.context.line_to(x, self.dimensions[VERT] - self.borders[VERT])
                    self.context.stroke()
                    next_x = x + width/2
                x += step
        else:
            lines = 11
            horizontal_step = float(self.plot_dimensions[HORZ])/(lines-1)
            x = self.borders[HORZ]
            for y in xrange(0, lines):
                self.context.move_to(x, self.border)
                self.context.line_to(x, self.dimensions[VERT] - self.borders[VERT])
                self.context.stroke()
                x += horizontal_step

    def render_horz_labels(self):
        step = (self.dimensions[HORZ] - 2*self.borders[HORZ])/(len(self.labels[HORZ])-1)
        x = self.borders[HORZ]
        next_x = 0

        for item in self.labels[HORZ]:
            self.context.set_source_rgba(*self.label_color)
            width = self.context.text_extents(item)[2]
            if x - width/2 > next_x and x - width/2 > self.border:
                self.context.move_to(x - width/2, self.dimensions[VERT] - self.borders[VERT] + self.max_value[HORZ] + 3)
                self.context.show_text(item)
                next_x = x + width/2
            x += step

    def render_vert_labels(self):
        series_length = len(self.labels[VERT])
        step = (self.plot_dimensions[VERT] - (series_length + 1)*self.space)/(len(self.labels[VERT]))
        y = self.border + step/2 + self.space

        for item in self.labels[VERT]:
            self.context.set_source_rgba(*self.label_color)
            width, height = self.context.text_extents(item)[2:4]
            self.context.move_to(self.borders[HORZ] - width - 5, y + height/2)
            self.context.show_text(item)
            y += step + self.space
        self.labels[VERT].reverse()

    def render_values(self):
        self.context.set_source_rgba(*self.value_label_color)
        self.context.set_font_size(self.font_size * 0.8)
        if self.stack:
            for i,group in enumerate(self.series):
                value = sum(group.to_list())
                height = self.context.text_extents(str(value))[3]
                x = self.borders[HORZ] + value*self.steps[HORZ] + 2
                y = self.borders[VERT] + (i+0.5)*self.steps[VERT] + (i+1)*self.space + height/2
                self.context.move_to(x, y)
                self.context.show_text(str(value))
        else:
            for i,group in enumerate(self.series):
                inner_step = self.steps[VERT]/len(group)
                y0 = self.border + i*self.steps[VERT] + (i+1)*self.space
                for number,data in enumerate(group):
                    height = self.context.text_extents(str(data.content))[3]
                    self.context.move_to(self.borders[HORZ] + data.content*self.steps[HORZ] + 2, y0 + 0.5*inner_step + height/2, )
                    self.context.show_text(str(data.content))
                    y0 += inner_step

    def render_plot(self):
        if self.stack:
            for i,group in enumerate(self.series):
                x0 = self.borders[HORZ]
                y0 = self.borders[VERT] + i*self.steps[VERT] + (i+1)*self.space
                for number,data in enumerate(group):
                    if self.series_colors[number][4] in ('radial','linear') :
                        linear = cairo.LinearGradient( data.content*self.steps[HORZ]/2, y0, data.content*self.steps[HORZ]/2, y0 + self.steps[VERT] )
                        color = self.series_colors[number]
                        linear.add_color_stop_rgba(0.0, 3.5*color[0]/5.0, 3.5*color[1]/5.0, 3.5*color[2]/5.0,1.0)
                        linear.add_color_stop_rgba(1.0, *color[:4])
                        self.context.set_source(linear)
                    elif self.series_colors[number][4] == 'solid':
                        self.context.set_source_rgba(*self.series_colors[number][:4])
                    if self.rounded_corners:
                        self.draw_rectangle(number, len(group), x0, y0, x0+data.content*self.steps[HORZ], y0+self.steps[VERT])
                        self.context.fill()
                    else:
                        self.context.rectangle(x0, y0, data.content*self.steps[HORZ], self.steps[VERT])
                        self.context.fill()
                    x0 += data.content*self.steps[HORZ]
        else:
            for i,group in enumerate(self.series):
                inner_step = self.steps[VERT]/len(group)
                x0 = self.borders[HORZ]
                y0 = self.border + i*self.steps[VERT] + (i+1)*self.space
                for number,data in enumerate(group):
                    linear = cairo.LinearGradient(data.content*self.steps[HORZ]/2, y0, data.content*self.steps[HORZ]/2, y0 + inner_step)
                    color = self.series_colors[number]
                    linear.add_color_stop_rgba(0.0, 3.5*color[0]/5.0, 3.5*color[1]/5.0, 3.5*color[2]/5.0,1.0)
                    linear.add_color_stop_rgba(1.0, *color[:4])
                    self.context.set_source(linear)
                    if self.rounded_corners and data.content != 0:
                        BarPlot.draw_round_rectangle(self,x0, y0, x0 + data.content*self.steps[HORZ], y0 + inner_step)
                        self.context.fill()
                    else:
                        self.context.rectangle(x0, y0, data.content*self.steps[HORZ], inner_step)
                        self.context.fill()
                    y0 += inner_step

class VerticalBarPlot(BarPlot):
    def __init__(self,
                 surface = None,
                 data = None,
                 width = 640,
                 height = 480,
                 background = "white light_gray",
                 border = 0,
                 display_values = False,
                 grid = False,
                 rounded_corners = False,
                 stack = False,
                 three_dimension = False,
                 series_labels = None,
                 x_labels = None,
                 y_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 series_colors = None):

        BarPlot.__init__(self, surface, data, width, height, background, border,
                         display_values, grid, rounded_corners, stack, three_dimension,
                         x_labels, y_labels, x_bounds, y_bounds, series_colors, VERT)
        self.series_labels = series_labels

    def calc_vert_extents(self):
        self.calc_extents(VERT)
        if self.labels[VERT] and not self.labels[HORZ]:
            self.borders[VERT] += 10

    def draw_rectangle_bottom(self, x0, y0, x1, y1):
        self.context.move_to(x1,y1)
        self.context.arc(x1-5, y1-5, 5, 0, math.pi/2)
        self.context.line_to(x0+5, y1)
        self.context.arc(x0+5, y1-5, 5, math.pi/2, math.pi)
        self.context.line_to(x0, y0)
        self.context.line_to(x1, y0)
        self.context.line_to(x1, y1)
        self.context.close_path()

    def draw_rectangle_top(self, x0, y0, x1, y1):
        self.context.arc(x0+5, y0+5, 5, -math.pi, -math.pi/2)
        self.context.line_to(x1-5, y0)
        self.context.arc(x1-5, y0+5, 5, -math.pi/2, 0)
        self.context.line_to(x1, y1)
        self.context.line_to(x0, y1)
        self.context.line_to(x0, y0)
        self.context.close_path()

    def draw_rectangle(self, index, length, x0, y0, x1, y1):
        if length == 1:
            BarPlot.draw_rectangle(self, x0, y0, x1, y1)
        elif index == 0:
            self.draw_rectangle_bottom(x0, y0, x1, y1)
        elif index == length-1:
            self.draw_rectangle_top(x0, y0, x1, y1)
        else:
            self.context.rectangle(x0, y0, x1-x0, y1-y0)

    def render_grid(self):
        self.context.set_source_rgba(0.8, 0.8, 0.8)
        if self.labels[VERT]:
            lines = len(self.labels[VERT])
            vertical_step = float(self.plot_dimensions[self.main_dir])/(lines-1)
            y = self.borders[VERT] + self.value_label
        else:
            lines = 11
            vertical_step = float(self.plot_dimensions[self.main_dir])/(lines-1)
            y = 1.2*self.border + self.value_label
        for x in xrange(0, lines):
            self.context.move_to(self.borders[HORZ], y)
            self.context.line_to(self.dimensions[HORZ] - self.border, y)
            self.context.stroke()
            y += vertical_step

    def render_ground(self):
        self.draw_3d_rectangle_front(self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT],
                                     self.dimensions[HORZ] - self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT] + 5, 10)
        self.context.fill()

        self.draw_3d_rectangle_side (self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT],
                                     self.dimensions[HORZ] - self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT] + 5, 10)
        self.context.fill()

        self.draw_3d_rectangle_top  (self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT],
                                     self.dimensions[HORZ] - self.borders[HORZ], self.dimensions[VERT] - self.borders[VERT] + 5, 10)
        self.context.fill()

    def render_horz_labels(self):
        series_length = len(self.labels[HORZ])
        step = float (self.plot_dimensions[HORZ] - (series_length + 1)*self.space)/len(self.labels[HORZ])
        x = self.borders[HORZ] + step/2 + self.space
        next_x = 0

        for item in self.labels[HORZ]:
            self.context.set_source_rgba(*self.label_color)
            width = self.context.text_extents(item)[2]
            if x - width/2 > next_x and x - width/2 > self.borders[HORZ]:
                self.context.move_to(x - width/2, self.dimensions[VERT] - self.borders[VERT] + self.max_value[HORZ] + 3)
                self.context.show_text(item)
                next_x = x + width/2
            x += step + self.space

    def render_vert_labels(self):
        self.context.set_source_rgba(*self.label_color)
        y = self.borders[VERT] + self.value_label
        step = (self.dimensions[VERT] - 2*self.borders[VERT] - self.value_label)/(len(self.labels[VERT]) - 1)
        self.labels[VERT].reverse()
        for item in self.labels[VERT]:
            width, height = self.context.text_extents(item)[2:4]
            self.context.move_to(self.borders[HORZ] - width - 5, y + height/2)
            self.context.show_text(item)
            y += step
        self.labels[VERT].reverse()

    def render_values(self):
        self.context.set_source_rgba(*self.value_label_color)
        self.context.set_font_size(self.font_size * 0.8)
        if self.stack:
            for i,group in enumerate(self.series):
                value = sum(group.to_list())
                width = self.context.text_extents(str(value))[2]
                x = self.borders[HORZ] + (i+0.5)*self.steps[HORZ] + (i+1)*self.space - width/2
                y = value*self.steps[VERT] + 2
                self.context.move_to(x, self.plot_top-y)
                self.context.show_text(str(value))
        else:
            for i,group in enumerate(self.series):
                inner_step = self.steps[HORZ]/len(group)
                x0 = self.borders[HORZ] + i*self.steps[HORZ] + (i+1)*self.space
                for number,data in enumerate(group):
                    width = self.context.text_extents(str(data.content))[2]
                    self.context.move_to(x0 + 0.5*inner_step - width/2, self.plot_top - data.content*self.steps[VERT] - 2)
                    self.context.show_text(str(data.content))
                    x0 += inner_step

    def render_plot(self):
        if self.stack:
            for i,group in enumerate(self.series):
                x0 = self.borders[HORZ] + i*self.steps[HORZ] + (i+1)*self.space
                y0 = 0
                for number,data in enumerate(group):
                    if self.series_colors[number][4] in ('linear','radial'):
                        linear = cairo.LinearGradient( x0, data.content*self.steps[VERT]/2, x0 + self.steps[HORZ], data.content*self.steps[VERT]/2 )
                        color = self.series_colors[number]
                        linear.add_color_stop_rgba(0.0, 3.5*color[0]/5.0, 3.5*color[1]/5.0, 3.5*color[2]/5.0,1.0)
                        linear.add_color_stop_rgba(1.0, *color[:4])
                        self.context.set_source(linear)
                    elif self.series_colors[number][4] == 'solid':
                        self.context.set_source_rgba(*self.series_colors[number][:4])
                    if self.rounded_corners:
                        self.draw_rectangle(number, len(group), x0, self.plot_top - y0 - data.content*self.steps[VERT], x0 + self.steps[HORZ], self.plot_top - y0)
                        self.context.fill()
                    else:
                        self.context.rectangle(x0, self.plot_top - y0 - data.content*self.steps[VERT], self.steps[HORZ], data.content*self.steps[VERT])
                        self.context.fill()
                    y0 += data.content*self.steps[VERT]
        else:
            for i,group in enumerate(self.series):
                inner_step = self.steps[HORZ]/len(group)
                y0 = self.borders[VERT]
                x0 = self.borders[HORZ] + i*self.steps[HORZ] + (i+1)*self.space
                for number,data in enumerate(group):
                    if self.series_colors[number][4] == 'linear':
                        linear = cairo.LinearGradient( x0, data.content*self.steps[VERT]/2, x0 + inner_step, data.content*self.steps[VERT]/2 )
                        color = self.series_colors[number]
                        linear.add_color_stop_rgba(0.0, 3.5*color[0]/5.0, 3.5*color[1]/5.0, 3.5*color[2]/5.0,1.0)
                        linear.add_color_stop_rgba(1.0, *color[:4])
                        self.context.set_source(linear)
                    elif self.series_colors[number][4] == 'solid':
                        self.context.set_source_rgba(*self.series_colors[number][:4])
                    if self.rounded_corners and data.content != 0:
                        BarPlot.draw_round_rectangle(self, x0, self.plot_top - data.content*self.steps[VERT], x0+inner_step, self.plot_top)
                        self.context.fill()
                    elif self.three_dimension:
                        self.draw_3d_rectangle_front(x0, self.plot_top - data.content*self.steps[VERT], x0+inner_step, self.plot_top, 5)
                        self.context.fill()
                        self.draw_3d_rectangle_side(x0, self.plot_top - data.content*self.steps[VERT], x0+inner_step, self.plot_top, 5)
                        self.context.fill()
                        self.draw_3d_rectangle_top(x0, self.plot_top - data.content*self.steps[VERT], x0+inner_step, self.plot_top, 5)
                        self.context.fill()
                    else:
                        self.context.rectangle(x0, self.plot_top - data.content*self.steps[VERT], inner_step, data.content*self.steps[VERT])
                        self.context.fill()

                    x0 += inner_step

class StreamChart(VerticalBarPlot):
    def __init__(self,
                 surface = None,
                 data = None,
                 width = 640,
                 height = 480,
                 background = "white light_gray",
                 border = 0,
                 grid = False,
                 series_legend = None,
                 x_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 series_colors = None):

        VerticalBarPlot.__init__(self, surface, data, width, height, background, border,
                                 False, grid, False, True, False,
                                 None, x_labels, None, x_bounds, y_bounds, series_colors)

    def calc_steps(self):
        other_dir = other_direction(self.main_dir)
        self.series_amplitude = self.bounds[self.main_dir][1] - self.bounds[self.main_dir][0]
        if self.series_amplitude:
            self.steps[self.main_dir] = float(self.plot_dimensions[self.main_dir])/self.series_amplitude
        else:
            self.steps[self.main_dir] = 0.00
        series_length = len(self.data)
        self.steps[other_dir] = float(self.plot_dimensions[other_dir])/series_length

    def render_legend(self):
        pass

    def ground(self, index):
        sum_values = sum(self.data[index])
        return -0.5*sum_values

    def calc_angles(self):
        middle = self.plot_top - self.plot_dimensions[VERT]/2.0
        self.angles = [tuple([0.0 for x in range(len(self.data)+1)])]
        for x_index in range(1, len(self.data)-1):
            t = []
            x0 = self.borders[HORZ] + (0.5 + x_index - 1)*self.steps[HORZ]
            x2 = self.borders[HORZ] + (0.5 + x_index + 1)*self.steps[HORZ]
            y0 = middle - self.ground(x_index-1)*self.steps[VERT]
            y2 = middle - self.ground(x_index+1)*self.steps[VERT]
            t.append(math.atan(float(y0-y2)/(x0-x2)))
            for data_index in range(len(self.data[x_index])):
                x0 = self.borders[HORZ] + (0.5 + x_index - 1)*self.steps[HORZ]
                x2 = self.borders[HORZ] + (0.5 + x_index + 1)*self.steps[HORZ]
                y0 = middle - self.ground(x_index-1)*self.steps[VERT] - self.data[x_index-1][data_index]*self.steps[VERT]
                y2 = middle - self.ground(x_index+1)*self.steps[VERT] - self.data[x_index+1][data_index]*self.steps[VERT]

                for i in range(0,data_index):
                    y0 -= self.data[x_index-1][i]*self.steps[VERT]
                    y2 -= self.data[x_index+1][i]*self.steps[VERT]

                if data_index == len(self.data[0])-1 and False:
                    self.context.set_source_rgba(0.0,0.0,0.0,0.3)
                    self.context.move_to(x0,y0)
                    self.context.line_to(x2,y2)
                    self.context.stroke()
                    self.context.arc(x0,y0,2,0,2*math.pi)
                    self.context.fill()
                t.append(math.atan(float(y0-y2)/(x0-x2)))
            self.angles.append(tuple(t))
        self.angles.append(tuple([0.0 for x in range(len(self.data)+1)]))

    def render_plot(self):
        self.calc_angles()
        middle = self.plot_top - self.plot_dimensions[VERT]/2.0
        p = 0.4*self.steps[HORZ]
        for data_index in range(len(self.data[0])-1,-1,-1):
            self.context.set_source_rgba(*self.series_colors[data_index][:4])

            #draw the upper line
            for x_index in range(len(self.data)-1) :
                x1 = self.borders[HORZ] + (0.5 + x_index)*self.steps[HORZ]
                y1 = middle - self.ground(x_index)*self.steps[VERT] - self.data[x_index][data_index]*self.steps[VERT]
                x2 = self.borders[HORZ] + (0.5 + x_index + 1)*self.steps[HORZ]
                y2 = middle - self.ground(x_index + 1)*self.steps[VERT] - self.data[x_index + 1][data_index]*self.steps[VERT]

                for i in range(0,data_index):
                    y1 -= self.data[x_index][i]*self.steps[VERT]
                    y2 -= self.data[x_index+1][i]*self.steps[VERT]

                if x_index == 0:
                    self.context.move_to(x1,y1)

                ang1 = self.angles[x_index][data_index+1]
                ang2 = self.angles[x_index+1][data_index+1] + math.pi
                self.context.curve_to(x1+p*math.cos(ang1),y1+p*math.sin(ang1),
                                      x2+p*math.cos(ang2),y2+p*math.sin(ang2),
                                      x2,y2)

            for x_index in range(len(self.data)-1,0,-1) :
                x1 = self.borders[HORZ] + (0.5 + x_index)*self.steps[HORZ]
                y1 = middle - self.ground(x_index)*self.steps[VERT]
                x2 = self.borders[HORZ] + (0.5 + x_index - 1)*self.steps[HORZ]
                y2 = middle - self.ground(x_index - 1)*self.steps[VERT]

                for i in range(0,data_index):
                    y1 -= self.data[x_index][i]*self.steps[VERT]
                    y2 -= self.data[x_index-1][i]*self.steps[VERT]

                if x_index == len(self.data)-1:
                    self.context.line_to(x1,y1+2)

                #revert angles by pi degrees to take the turn back
                ang1 = self.angles[x_index][data_index] + math.pi
                ang2 = self.angles[x_index-1][data_index]
                self.context.curve_to(x1+p*math.cos(ang1),y1+p*math.sin(ang1),
                                      x2+p*math.cos(ang2),y2+p*math.sin(ang2),
                                      x2,y2+2)

            self.context.close_path()
            self.context.fill()

            if False:
                self.context.move_to(self.borders[HORZ] + 0.5*self.steps[HORZ], middle)
                for x_index in range(len(self.data)-1) :
                    x1 = self.borders[HORZ] + (0.5 + x_index)*self.steps[HORZ]
                    y1 = middle - self.ground(x_index)*self.steps[VERT] - self.data[x_index][data_index]*self.steps[VERT]
                    x2 = self.borders[HORZ] + (0.5 + x_index + 1)*self.steps[HORZ]
                    y2 = middle - self.ground(x_index + 1)*self.steps[VERT] - self.data[x_index + 1][data_index]*self.steps[VERT]

                    for i in range(0,data_index):
                        y1 -= self.data[x_index][i]*self.steps[VERT]
                        y2 -= self.data[x_index+1][i]*self.steps[VERT]

                    ang1 = self.angles[x_index][data_index+1]
                    ang2 = self.angles[x_index+1][data_index+1] + math.pi
                    self.context.set_source_rgba(1.0,0.0,0.0)
                    self.context.arc(x1+p*math.cos(ang1),y1+p*math.sin(ang1),2,0,2*math.pi)
                    self.context.fill()
                    self.context.set_source_rgba(0.0,0.0,0.0)
                    self.context.arc(x2+p*math.cos(ang2),y2+p*math.sin(ang2),2,0,2*math.pi)
                    self.context.fill()
                    '''self.context.set_source_rgba(0.0,0.0,0.0,0.3)
                    self.context.arc(x2,y2,2,0,2*math.pi)
                    self.context.fill()'''
                    self.context.move_to(x1,y1)
                    self.context.line_to(x1+p*math.cos(ang1),y1+p*math.sin(ang1))
                    self.context.stroke()
                    self.context.move_to(x2,y2)
                    self.context.line_to(x2+p*math.cos(ang2),y2+p*math.sin(ang2))
                    self.context.stroke()
            if False:
                for x_index in range(len(self.data)-1,0,-1) :
                    x1 = self.borders[HORZ] + (0.5 + x_index)*self.steps[HORZ]
                    y1 = middle - self.ground(x_index)*self.steps[VERT]
                    x2 = self.borders[HORZ] + (0.5 + x_index - 1)*self.steps[HORZ]
                    y2 = middle - self.ground(x_index - 1)*self.steps[VERT]

                    for i in range(0,data_index):
                        y1 -= self.data[x_index][i]*self.steps[VERT]
                        y2 -= self.data[x_index-1][i]*self.steps[VERT]

                    #revert angles by pi degrees to take the turn back
                    ang1 = self.angles[x_index][data_index] + math.pi
                    ang2 = self.angles[x_index-1][data_index]
                    self.context.set_source_rgba(0.0,1.0,0.0)
                    self.context.arc(x1+p*math.cos(ang1),y1+p*math.sin(ang1),2,0,2*math.pi)
                    self.context.fill()
                    self.context.set_source_rgba(0.0,0.0,1.0)
                    self.context.arc(x2+p*math.cos(ang2),y2+p*math.sin(ang2),2,0,2*math.pi)
                    self.context.fill()
                    '''self.context.set_source_rgba(0.0,0.0,0.0,0.3)
                    self.context.arc(x2,y2,2,0,2*math.pi)
                    self.context.fill()'''
                    self.context.move_to(x1,y1)
                    self.context.line_to(x1+p*math.cos(ang1),y1+p*math.sin(ang1))
                    self.context.stroke()
                    self.context.move_to(x2,y2)
                    self.context.line_to(x2+p*math.cos(ang2),y2+p*math.sin(ang2))
                    self.context.stroke()
            #break

            #self.context.arc(self.dimensions[HORZ]/2, self.dimensions[VERT]/2,50,0,3*math.pi/2)
            #self.context.fill()


class PiePlot(Plot):
    #TODO: Check the old cairoplot, graphs aren't matching
    def __init__ (self,
            surface = None,
            data = None,
            width = 640,
            height = 480,
            background = "white light_gray",
            gradient = False,
            shadow = False,
            colors = None):

        Plot.__init__( self, surface, data, width, height, background, series_colors = colors )
        self.center = (self.dimensions[HORZ]/2, self.dimensions[VERT]/2)
        self.total = sum( self.series.to_list() )
        self.radius = min(self.dimensions[HORZ]/3,self.dimensions[VERT]/3)
        self.gradient = gradient
        self.shadow = shadow

    def sort_function(x,y):
        return x.content - y.content

    def load_series(self, data, x_labels=None, y_labels=None, series_colors=None):
        Plot.load_series(self, data, x_labels, y_labels, series_colors)
        # Already done inside series
        #self.data = sorted(self.data)

    def draw_piece(self, angle, next_angle):
        self.context.move_to(self.center[0],self.center[1])
        self.context.line_to(self.center[0] + self.radius*math.cos(angle), self.center[1] + self.radius*math.sin(angle))
        self.context.arc(self.center[0], self.center[1], self.radius, angle, next_angle)
        self.context.line_to(self.center[0], self.center[1])
        self.context.close_path()

    def render(self):
        self.render_background()
        self.render_bounding_box()
        if self.shadow:
            self.render_shadow()
        self.render_plot()
        self.render_series_labels()

    def render_shadow(self):
        horizontal_shift = 3
        vertical_shift = 3
        self.context.set_source_rgba(0, 0, 0, 0.5)
        self.context.arc(self.center[0] + horizontal_shift, self.center[1] + vertical_shift, self.radius, 0, 2*math.pi)
        self.context.fill()

    def render_series_labels(self):
        angle = 0
        next_angle = 0
        x0,y0 = self.center
        cr = self.context
        for number,key in enumerate(self.series_labels):
            # self.data[number] should be just a number
            data = sum(self.series[number].to_list())

            next_angle = angle + 2.0*math.pi*data/self.total
            cr.set_source_rgba(*self.series_colors[number][:4])
            w = cr.text_extents(key)[2]
            if (angle + next_angle)/2 < math.pi/2 or (angle + next_angle)/2 > 3*math.pi/2:
                cr.move_to(x0 + (self.radius+10)*math.cos((angle+next_angle)/2), y0 + (self.radius+10)*math.sin((angle+next_angle)/2) )
            else:
                cr.move_to(x0 + (self.radius+10)*math.cos((angle+next_angle)/2) - w, y0 + (self.radius+10)*math.sin((angle+next_angle)/2) )
            cr.show_text(key)
            angle = next_angle

    def render_plot(self):
        angle = 0
        next_angle = 0
        x0,y0 = self.center
        cr = self.context
        for number,group in enumerate(self.series):
            # Group should be just a number
            data = sum(group.to_list())
            next_angle = angle + 2.0*math.pi*data/self.total
            if self.gradient or self.series_colors[number][4] in ('linear','radial'):
                gradient_color = cairo.RadialGradient(self.center[0], self.center[1], 0, self.center[0], self.center[1], self.radius)
                gradient_color.add_color_stop_rgba(0.3, *self.series_colors[number][:4])
                gradient_color.add_color_stop_rgba(1, self.series_colors[number][0]*0.7,
                                                      self.series_colors[number][1]*0.7,
                                                      self.series_colors[number][2]*0.7,
                                                      self.series_colors[number][3])
                cr.set_source(gradient_color)
            else:
                cr.set_source_rgba(*self.series_colors[number][:4])

            self.draw_piece(angle, next_angle)
            cr.fill()

            cr.set_source_rgba(1.0, 1.0, 1.0)
            self.draw_piece(angle, next_angle)
            cr.stroke()

            angle = next_angle

class DonutPlot(PiePlot):
    def __init__ (self,
            surface = None,
            data = None,
            width = 640,
            height = 480,
            background = "white light_gray",
            gradient = False,
            shadow = False,
            colors = None,
            inner_radius=-1):

        Plot.__init__( self, surface, data, width, height, background, series_colors = colors )

        self.center = ( self.dimensions[HORZ]/2, self.dimensions[VERT]/2 )
        self.total = sum( self.series.to_list() )
        self.radius = min( self.dimensions[HORZ]/3,self.dimensions[VERT]/3 )
        self.inner_radius = inner_radius*self.radius

        if inner_radius == -1:
            self.inner_radius = self.radius/3

        self.gradient = gradient
        self.shadow = shadow

    def draw_piece(self, angle, next_angle):
        self.context.move_to(self.center[0] + (self.inner_radius)*math.cos(angle), self.center[1] + (self.inner_radius)*math.sin(angle))
        self.context.line_to(self.center[0] + self.radius*math.cos(angle), self.center[1] + self.radius*math.sin(angle))
        self.context.arc(self.center[0], self.center[1], self.radius, angle, next_angle)
        self.context.line_to(self.center[0] + (self.inner_radius)*math.cos(next_angle), self.center[1] + (self.inner_radius)*math.sin(next_angle))
        self.context.arc_negative(self.center[0], self.center[1], self.inner_radius, next_angle, angle)
        self.context.close_path()

    def render_shadow(self):
        horizontal_shift = 3
        vertical_shift = 3
        self.context.set_source_rgba(0, 0, 0, 0.5)
        self.context.arc(self.center[0] + horizontal_shift, self.center[1] + vertical_shift, self.inner_radius, 0, 2*math.pi)
        self.context.arc_negative(self.center[0] + horizontal_shift, self.center[1] + vertical_shift, self.radius, 0, -2*math.pi)
        self.context.fill()

class GanttChart (Plot) :
    def __init__(self,
                 surface = None,
                 data = None,
                 width = 640,
                 height = 480,
                 x_labels = None,
                 y_labels = None,
                 colors = None):
        self.bounds = {}
        self.max_value = {}
        Plot.__init__(self, surface, data, width, height,  x_labels = x_labels, y_labels = y_labels, series_colors = colors)

    def load_series(self, data, x_labels=None, y_labels=None, series_colors=None):
        Plot.load_series(self, data, x_labels, y_labels, series_colors)
        self.calc_boundaries()

    def calc_boundaries(self):
        self.bounds[HORZ] = (0,len(self.series))
        end_pos = max(self.series.to_list())

        #for group in self.series:
        #    if hasattr(item, "__delitem__"):
        #        for sub_item in item:
        #            end_pos = max(sub_item)
        #    else:
        #        end_pos = max(item)
        self.bounds[VERT] = (0,end_pos)

    def calc_extents(self, direction):
        self.max_value[direction] = 0
        if self.labels[direction]:
            self.max_value[direction] = max(self.context.text_extents(item)[2] for item in self.labels[direction])
        else:
            self.max_value[direction] = self.context.text_extents( str(self.bounds[direction][1] + 1) )[2]

    def calc_horz_extents(self):
        self.calc_extents(HORZ)
        self.borders[HORZ] = 100 + self.max_value[HORZ]

    def calc_vert_extents(self):
        self.calc_extents(VERT)
        self.borders[VERT] = self.dimensions[VERT]/(self.bounds[HORZ][1] + 1)

    def calc_steps(self):
        self.horizontal_step = (self.dimensions[HORZ] - self.borders[HORZ])/(len(self.labels[VERT]))
        self.vertical_step = self.borders[VERT]

    def render(self):
        self.calc_horz_extents()
        self.calc_vert_extents()
        self.calc_steps()
        self.render_background()

        self.render_labels()
        self.render_grid()
        self.render_plot()

    def render_background(self):
        cr = self.context
        cr.set_source_rgba(255,255,255)
        cr.rectangle(0,0,self.dimensions[HORZ], self.dimensions[VERT])
        cr.fill()
        for number,group in enumerate(self.series):
            linear = cairo.LinearGradient(self.dimensions[HORZ]/2, self.borders[VERT] + number*self.vertical_step,
                                          self.dimensions[HORZ]/2, self.borders[VERT] + (number+1)*self.vertical_step)
            linear.add_color_stop_rgba(0,1.0,1.0,1.0,1.0)
            linear.add_color_stop_rgba(1.0,0.9,0.9,0.9,1.0)
            cr.set_source(linear)
            cr.rectangle(0,self.borders[VERT] + number*self.vertical_step,self.dimensions[HORZ],self.vertical_step)
            cr.fill()

    def render_grid(self):
        cr = self.context
        cr.set_source_rgba(0.7, 0.7, 0.7)
        cr.set_dash((1,0,0,0,0,0,1))
        cr.set_line_width(0.5)
        for number,label in enumerate(self.labels[VERT]):
            h = cr.text_extents(label)[3]
            cr.move_to(self.borders[HORZ] + number*self.horizontal_step, self.vertical_step/2 + h)
            cr.line_to(self.borders[HORZ] + number*self.horizontal_step, self.dimensions[VERT])
        cr.stroke()

    def render_labels(self):
        self.context.set_font_size(0.02 * self.dimensions[HORZ])

        self.render_horz_labels()
        self.render_vert_labels()

    def render_horz_labels(self):
        cr = self.context
        labels = self.labels[HORZ]
        if not labels:
            labels = [str(i) for i in range(1, self.bounds[HORZ][1] + 1)  ]
        for number,label in enumerate(labels):
            if label != None:
                cr.set_source_rgba(0.5, 0.5, 0.5)
                w,h = cr.text_extents(label)[2], cr.text_extents(label)[3]
                cr.move_to(40,self.borders[VERT] + number*self.vertical_step + self.vertical_step/2 + h/2)
                cr.show_text(label)

    def render_vert_labels(self):
        cr = self.context
        labels = self.labels[VERT]
        if not labels:
            labels = [str(i) for i in range(1, self.bounds[VERT][1] + 1)  ]
        for number,label in enumerate(labels):
            w,h = cr.text_extents(label)[2], cr.text_extents(label)[3]
            cr.move_to(self.borders[HORZ] + number*self.horizontal_step - w/2, self.vertical_step/2)
            cr.show_text(label)

    def render_rectangle(self, x0, y0, x1, y1, color):
        self.draw_shadow(x0, y0, x1, y1)
        self.draw_rectangle(x0, y0, x1, y1, color)

    def draw_rectangular_shadow(self, gradient, x0, y0, w, h):
        self.context.set_source(gradient)
        self.context.rectangle(x0,y0,w,h)
        self.context.fill()

    def draw_circular_shadow(self, x, y, radius, ang_start, ang_end, mult, shadow):
        gradient = cairo.RadialGradient(x, y, 0, x, y, 2*radius)
        gradient.add_color_stop_rgba(0, 0, 0, 0, shadow)
        gradient.add_color_stop_rgba(1, 0, 0, 0, 0)
        self.context.set_source(gradient)
        self.context.move_to(x,y)
        self.context.line_to(x + mult[0]*radius,y + mult[1]*radius)
        self.context.arc(x, y, 8, ang_start, ang_end)
        self.context.line_to(x,y)
        self.context.close_path()
        self.context.fill()

    def draw_rectangle(self, x0, y0, x1, y1, color):
        cr = self.context
        middle = (x0+x1)/2
        linear = cairo.LinearGradient(middle,y0,middle,y1)
        linear.add_color_stop_rgba(0,3.5*color[0]/5.0, 3.5*color[1]/5.0, 3.5*color[2]/5.0,1.0)
        linear.add_color_stop_rgba(1,*color[:4])
        cr.set_source(linear)

        cr.arc(x0+5, y0+5, 5, 0, 2*math.pi)
        cr.arc(x1-5, y0+5, 5, 0, 2*math.pi)
        cr.arc(x0+5, y1-5, 5, 0, 2*math.pi)
        cr.arc(x1-5, y1-5, 5, 0, 2*math.pi)
        cr.rectangle(x0+5,y0,x1-x0-10,y1-y0)
        cr.rectangle(x0,y0+5,x1-x0,y1-y0-10)
        cr.fill()

    def draw_shadow(self, x0, y0, x1, y1):
        shadow = 0.4
        h_mid = (x0+x1)/2
        v_mid = (y0+y1)/2
        h_linear_1 = cairo.LinearGradient(h_mid,y0-4,h_mid,y0+4)
        h_linear_2 = cairo.LinearGradient(h_mid,y1-4,h_mid,y1+4)
        v_linear_1 = cairo.LinearGradient(x0-4,v_mid,x0+4,v_mid)
        v_linear_2 = cairo.LinearGradient(x1-4,v_mid,x1+4,v_mid)

        h_linear_1.add_color_stop_rgba( 0, 0, 0, 0, 0)
        h_linear_1.add_color_stop_rgba( 1, 0, 0, 0, shadow)
        h_linear_2.add_color_stop_rgba( 0, 0, 0, 0, shadow)
        h_linear_2.add_color_stop_rgba( 1, 0, 0, 0, 0)
        v_linear_1.add_color_stop_rgba( 0, 0, 0, 0, 0)
        v_linear_1.add_color_stop_rgba( 1, 0, 0, 0, shadow)
        v_linear_2.add_color_stop_rgba( 0, 0, 0, 0, shadow)
        v_linear_2.add_color_stop_rgba( 1, 0, 0, 0, 0)

        self.draw_rectangular_shadow(h_linear_1,x0+4,y0-4,x1-x0-8,8)
        self.draw_rectangular_shadow(h_linear_2,x0+4,y1-4,x1-x0-8,8)
        self.draw_rectangular_shadow(v_linear_1,x0-4,y0+4,8,y1-y0-8)
        self.draw_rectangular_shadow(v_linear_2,x1-4,y0+4,8,y1-y0-8)

        self.draw_circular_shadow(x0+4, y0+4, 4, math.pi, 3*math.pi/2, (-1,0), shadow)
        self.draw_circular_shadow(x1-4, y0+4, 4, 3*math.pi/2, 2*math.pi, (0,-1), shadow)
        self.draw_circular_shadow(x0+4, y1-4, 4, math.pi/2, math.pi, (0,1), shadow)
        self.draw_circular_shadow(x1-4, y1-4, 4, 0, math.pi/2, (1,0), shadow)

    def render_plot(self):
        for index,group in enumerate(self.series):
            for data in group:
                self.render_rectangle(self.borders[HORZ] + data.content[0]*self.horizontal_step,
                                      self.borders[VERT] + index*self.vertical_step + self.vertical_step/4.0,
                                      self.borders[HORZ] + data.content[1]*self.horizontal_step,
                                      self.borders[VERT] + index*self.vertical_step + 3.0*self.vertical_step/4.0,
                                      self.series_colors[index])

# Function definition

def scatter_plot(name,
                 data   = None,
                 errorx = None,
                 errory = None,
                 width  = 640,
                 height = 480,
                 background = "white light_gray",
                 border = 0,
                 axis = False,
                 dash = False,
                 discrete = False,
                 dots = False,
                 grid = False,
                 series_legend = False,
                 x_labels = None,
                 y_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 z_bounds = None,
                 x_title  = None,
                 y_title  = None,
                 series_colors = None,
                 circle_colors = None):

    '''
        - Function to plot scatter data.

        - Parameters

        data - The values to be ploted might be passed in a two basic:
               list of points:       [(0,0), (0,1), (0,2)] or [(0,0,1), (0,1,4), (0,2,1)]
               lists of coordinates: [ [0,0,0] , [0,1,2] ] or [ [0,0,0] , [0,1,2] , [1,4,1] ]
               Notice that these kinds of that can be grouped in order to form more complex data
               using lists of lists or dictionaries;
        series_colors - Define color values for each of the series
        circle_colors - Define a lower and an upper bound for the circle colors for variable radius
                        (3 dimensions) series
    '''

    plot = ScatterPlot( name, data, errorx, errory, width, height, background, border,
                        axis, dash, discrete, dots, grid, series_legend, x_labels, y_labels,
                        x_bounds, y_bounds, z_bounds, x_title, y_title, series_colors, circle_colors )
    plot.render()
    plot.commit()

def dot_line_plot(name,
                  data,
                  width,
                  height,
                  background = "white light_gray",
                  border = 0,
                  axis = False,
                  dash = False,
                  dots = False,
                  grid = False,
                  series_legend = False,
                  x_labels = None,
                  y_labels = None,
                  x_bounds = None,
                  y_bounds = None,
                  x_title  = None,
                  y_title  = None,
                  series_colors = None):
    '''
        - Function to plot graphics using dots and lines.

        dot_line_plot (name, data, width, height, background = "white light_gray", border = 0, axis = False, grid = False, x_labels = None, y_labels = None, x_bounds = None, y_bounds = None)

        - Parameters

        name - Name of the desired output file, no need to input the .svg as it will be added at runtim;
        data - The list, list of lists or dictionary holding the data to be plotted;
        width, height - Dimensions of the output image;
        background - A 3 element tuple representing the rgb color expected for the background or a new cairo linear gradient.
                     If left None, a gray to white gradient will be generated;
        border - Distance in pixels of a square border into which the graphics will be drawn;
        axis - Whether or not the axis are to be drawn;
        dash - Boolean or a list or a dictionary of booleans indicating whether or not the associated series should be drawn in dashed mode;
        dots - Whether or not dots should be drawn on each point;
        grid - Whether or not the gris is to be drawn;
        series_legend - Whether or not the legend is to be drawn;
        x_labels, y_labels - lists of strings containing the horizontal and vertical labels for the axis;
        x_bounds, y_bounds - tuples containing the lower and upper value bounds for the data to be plotted;
        x_title - Whether or not to plot a title over the x axis.
        y_title - Whether or not to plot a title over the y axis.

        - Examples of use

        data = [0, 1, 3, 8, 9, 0, 10, 10, 2, 1]
        CairoPlot.dot_line_plot('teste', data, 400, 300)

        data = { "john" : [10, 10, 10, 10, 30], "mary" : [0, 0, 3, 5, 15], "philip" : [13, 32, 11, 25, 2] }
        x_labels = ["jan/2008", "feb/2008", "mar/2008", "apr/2008", "may/2008" ]
        CairoPlot.dot_line_plot( 'test', data, 400, 300, axis = True, grid = True,
                                  series_legend = True, x_labels = x_labels )
    '''
    plot = DotLinePlot( name, data, width, height, background, border,
                        axis, dash, dots, grid, series_legend, x_labels, y_labels,
                        x_bounds, y_bounds, x_title, y_title, series_colors )
    plot.render()
    plot.commit()

def function_plot(name,
                  data,
                  width,
                  height,
                  background = "white light_gray",
                  border = 0,
                  axis = True,
                  dots = False,
                  discrete = False,
                  grid = False,
                  series_legend = False,
                  x_labels = None,
                  y_labels = None,
                  x_bounds = None,
                  y_bounds = None,
                  x_title  = None,
                  y_title  = None,
                  series_colors = None,
                  step = 1):

    '''
        - Function to plot functions.

        function_plot(name, data, width, height, background = "white light_gray", border = 0, axis = True, grid = False, dots = False, x_labels = None, y_labels = None, x_bounds = None, y_bounds = None, step = 1, discrete = False)

        - Parameters

        name - Name of the desired output file, no need to input the .svg as it will be added at runtim;
        data - The list, list of lists or dictionary holding the data to be plotted;
        width, height - Dimensions of the output image;
        background - A 3 element tuple representing the rgb color expected for the background or a new cairo linear gradient.
                     If left None, a gray to white gradient will be generated;
        border - Distance in pixels of a square border into which the graphics will be drawn;
        axis - Whether or not the axis are to be drawn;
        grid - Whether or not the gris is to be drawn;
        dots - Whether or not dots should be shown at each point;
        x_labels, y_labels - lists of strings containing the horizontal and vertical labels for the axis;
        x_bounds, y_bounds - tuples containing the lower and upper value bounds for the data to be plotted;
        step - the horizontal distance from one point to the other. The smaller, the smoother the curve will be;
        discrete - whether or not the function should be plotted in discrete format.

        - Example of use

        data = lambda x : x**2
        CairoPlot.function_plot('function4', data, 400, 300, grid = True, x_bounds=(-10,10), step = 0.1)
    '''

    plot = FunctionPlot( name, data, width, height, background, border,
                         axis, discrete, dots, grid, series_legend, x_labels, y_labels,
                         x_bounds, y_bounds, x_title, y_title, series_colors, step )
    plot.render()
    plot.commit()

def pie_plot( name, data, width, height, background = "white light_gray", gradient = False, shadow = False, colors = None ):

    '''
        - Function to plot pie graphics.

        pie_plot(name, data, width, height, background = "white light_gray", gradient = False, colors = None)

        - Parameters

        name - Name of the desired output file, no need to input the .svg as it will be added at runtim;
        data - The list, list of lists or dictionary holding the data to be plotted;
        width, height - Dimensions of the output image;
        background - A 3 element tuple representing the rgb color expected for the background or a new cairo linear gradient.
                     If left None, a gray to white gradient will be generated;
        gradient - Whether or not the pie color will be painted with a gradient;
        shadow - Whether or not there will be a shadow behind the pie;
        colors - List of slices colors.

        - Example of use

        teste_data = {"john" : 123, "mary" : 489, "philip" : 890 , "suzy" : 235}
        CairoPlot.pie_plot("pie_teste", teste_data, 500, 500)
    '''

    plot = PiePlot( name, data, width, height, background, gradient, shadow, colors )
    plot.render()
    plot.commit()

def donut_plot(name, data, width, height, background = "white light_gray", gradient = False, shadow = False, colors = None, inner_radius = -1):

    '''
        - Function to plot donut graphics.

        donut_plot(name, data, width, height, background = "white light_gray", gradient = False, inner_radius = -1)

        - Parameters

        name - Name of the desired output file, no need to input the .svg as it will be added at runtim;
        data - The list, list of lists or dictionary holding the data to be plotted;
        width, height - Dimensions of the output image;
        background - A 3 element tuple representing the rgb color expected for the background or a new cairo linear gradient.
                     If left None, a gray to white gradient will be generated;
        shadow - Whether or not there will be a shadow behind the donut;
        gradient - Whether or not the donut color will be painted with a gradient;
        colors - List of slices colors;
        inner_radius - The radius of the donut's inner circle.

        - Example of use

        teste_data = {"john" : 123, "mary" : 489, "philip" : 890 , "suzy" : 235}
        CairoPlot.donut_plot("donut_teste", teste_data, 500, 500)
    '''

    plot = DonutPlot(name, data, width, height, background, gradient, shadow, colors, inner_radius)
    plot.render()
    plot.commit()

def gantt_chart(name, pieces, width, height, x_labels, y_labels, colors):

    '''
        - Function to generate Gantt Charts.

        gantt_chart(name, pieces, width, height, x_labels, y_labels, colors):

        - Parameters

        name - Name of the desired output file, no need to input the .svg as it will be added at runtim;
        pieces - A list defining the spaces to be drawn. The user must pass, for each line, the index of its start and the index of its end. If a line must have two or more spaces, they must be passed inside a list;
        width, height - Dimensions of the output image;
        x_labels - A list of names for each of the vertical lines;
        y_labels - A list of names for each of the horizontal spaces;
        colors - List containing the colors expected for each of the horizontal spaces

        - Example of use

        pieces = [ (0.5,5.5) , [(0,4),(6,8)] , (5.5,7) , (7,8)]
        x_labels = [ 'teste01', 'teste02', 'teste03', 'teste04']
        y_labels = [ '0001', '0002', '0003', '0004', '0005', '0006', '0007', '0008', '0009', '0010' ]
        colors = [ (1.0, 0.0, 0.0), (1.0, 0.7, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0) ]
        CairoPlot.gantt_chart('gantt_teste', pieces, 600, 300, x_labels, y_labels, colors)
    '''

    plot = GanttChart(name, pieces, width, height, x_labels, y_labels, colors)
    plot.render()
    plot.commit()

def vertical_bar_plot(name,
                      data,
                      width,
                      height,
                      background = "white light_gray",
                      border = 0,
                      display_values = False,
                      grid = False,
                      rounded_corners = False,
                      stack = False,
                      three_dimension = False,
                      series_labels = None,
                      x_labels = None,
                      y_labels = None,
                      x_bounds = None,
                      y_bounds = None,
                      colors = None):
    #TODO: Fix docstring for vertical_bar_plot
    '''
        - Function to generate vertical Bar Plot Charts.

        bar_plot(name, data, width, height, background, border, grid, rounded_corners, three_dimension,
                 x_labels, y_labels, x_bounds, y_bounds, colors):

        - Parameters

        name - Name of the desired output file, no need to input the .svg as it will be added at runtime;
        data - The list, list of lists or dictionary holding the data to be plotted;
        width, height - Dimensions of the output image;
        background - A 3 element tuple representing the rgb color expected for the background or a new cairo linear gradient.
                     If left None, a gray to white gradient will be generated;
        border - Distance in pixels of a square border into which the graphics will be drawn;
        grid - Whether or not the gris is to be drawn;
        rounded_corners - Whether or not the bars should have rounded corners;
        three_dimension - Whether or not the bars should be drawn in pseudo 3D;
        x_labels, y_labels - lists of strings containing the horizontal and vertical labels for the axis;
        x_bounds, y_bounds - tuples containing the lower and upper value bounds for the data to be plotted;
        colors - List containing the colors expected for each of the bars.

        - Example of use

        data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
        CairoPlot.vertical_bar_plot ('bar2', data, 400, 300, border = 20, grid = True, rounded_corners = False)
    '''

    plot = VerticalBarPlot(name, data, width, height, background, border,
                           display_values, grid, rounded_corners, stack, three_dimension,
                           series_labels, x_labels, y_labels, x_bounds, y_bounds, colors)
    plot.render()
    plot.commit()

def horizontal_bar_plot(name,
                       data,
                       width,
                       height,
                       background = "white light_gray",
                       border = 0,
                       display_values = False,
                       grid = False,
                       rounded_corners = False,
                       stack = False,
                       three_dimension = False,
                       series_labels = None,
                       x_labels = None,
                       y_labels = None,
                       x_bounds = None,
                       y_bounds = None,
                       colors = None):

    #TODO: Fix docstring for horizontal_bar_plot
    '''
        - Function to generate Horizontal Bar Plot Charts.

        bar_plot(name, data, width, height, background, border, grid, rounded_corners, three_dimension,
                 x_labels, y_labels, x_bounds, y_bounds, colors):

        - Parameters

        name - Name of the desired output file, no need to input the .svg as it will be added at runtime;
        data - The list, list of lists or dictionary holding the data to be plotted;
        width, height - Dimensions of the output image;
        background - A 3 element tuple representing the rgb color expected for the background or a new cairo linear gradient.
                     If left None, a gray to white gradient will be generated;
        border - Distance in pixels of a square border into which the graphics will be drawn;
        grid - Whether or not the gris is to be drawn;
        rounded_corners - Whether or not the bars should have rounded corners;
        three_dimension - Whether or not the bars should be drawn in pseudo 3D;
        x_labels, y_labels - lists of strings containing the horizontal and vertical labels for the axis;
        x_bounds, y_bounds - tuples containing the lower and upper value bounds for the data to be plotted;
        colors - List containing the colors expected for each of the bars.

        - Example of use

        data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
        CairoPlot.bar_plot ('bar2', data, 400, 300, border = 20, grid = True, rounded_corners = False)
    '''

    plot = HorizontalBarPlot(name, data, width, height, background, border,
                             display_values, grid, rounded_corners, stack, three_dimension,
                             series_labels, x_labels, y_labels, x_bounds, y_bounds, colors)
    plot.render()
    plot.commit()

def stream_chart(name,
                 data,
                 width,
                 height,
                 background = "white light_gray",
                 border = 0,
                 grid = False,
                 series_legend = None,
                 x_labels = None,
                 x_bounds = None,
                 y_bounds = None,
                 colors = None):

    #TODO: Fix docstring for horizontal_bar_plot
    plot = StreamChart(name, data, width, height, background, border,
                       grid, series_legend, x_labels, x_bounds, y_bounds, colors)
    plot.render()
    plot.commit()


if __name__ == "__main__":
    import tests
    import seriestests
