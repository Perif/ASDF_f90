/******************************************************************************
 * Copyright 2015 ASDF developers
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
/** 
 * @file ASDF_write.c
 * @author Matthieu Lefebvre
 */

#include <mpi.h>
#include <hdf5.h>
#include <string.h>

#include "ASDF_common.h"
#include "ASDF_write.h"

hid_t ASDF_create_new_file(const char *filename, MPI_Comm comm) {
  hid_t plist_id, file_id;

  CHK_H5(plist_id = H5Pcreate(H5P_FILE_ACCESS));
  CHK_H5(H5Pset_fapl_mpio(plist_id, comm, MPI_INFO_NULL));
  /* Create the file collectively.*/
  CHK_H5(file_id = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, plist_id));
  CHK_H5(H5Pclose(plist_id));

  return file_id;
}

herr_t ASDF_write_string_attribute(hid_t dataset_id, 
                                   const char *attr_name, 
                                   const char *attr_value) {
  hid_t space_id, type_id, attr_id;

  CHK_H5(space_id  = H5Screate(H5S_SCALAR));
  CHK_H5(type_id = H5Tcopy(H5T_C_S1));
  CHK_H5(H5Tset_size(type_id, strlen(attr_value)+1));
  CHK_H5(H5Tset_strpad(type_id,H5T_STR_NULLTERM));

  CHK_H5(attr_id = H5Acreate(dataset_id, attr_name, type_id, space_id,
        H5P_DEFAULT, H5P_DEFAULT));

  CHK_H5(H5Awrite(attr_id, type_id, attr_value));

  CHK_H5(H5Aclose(attr_id));
  CHK_H5(H5Tclose(type_id));
  CHK_H5(H5Sclose(space_id));

  return 0; // Success
}

herr_t ASDF_write_auxiliary_data(hid_t loc_id) {
  hid_t group_id;
  CHK_H5(group_id = H5Gcreate(loc_id, "AuxiliaryData", 
        H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
  /* Fill up with whatever AuxiliaryData contains. */
  CHK_H5(H5Gclose(group_id));

  return 0; // Success
}

herr_t ASDF_write_provenance_data(hid_t loc_id) {
  hid_t group_id;
  CHK_H5(group_id = H5Gcreate(loc_id, "Provenance", 
        H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
  /* Fill up with whatever Provenance contains. */
  CHK_H5(H5Gclose(group_id));

  return 0; // Success
}

herr_t ASDF_write_quakeml(hid_t loc_id, const char *quakeml_string) {
  hsize_t dims[1] = {strlen(quakeml_string)+1};

  hid_t space_id, array_id;
  CHK_H5(space_id = H5Screate_simple(1, dims, NULL));

  CHK_H5(array_id = H5Dcreate(loc_id, "/QuakeML", H5T_STD_I8LE, space_id, 
        H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
  CHK_H5(H5Dwrite(array_id, H5T_STD_I8LE, H5S_ALL, H5S_ALL, 
        H5P_DEFAULT, quakeml_string));

  CHK_H5(H5Dclose(array_id));
  CHK_H5(H5Sclose(space_id));

  return 0; // Success
}

hid_t ASDF_create_waveforms_group(hid_t loc_id) {
  hid_t group_id;
  CHK_H5(group_id = H5Gcreate(loc_id, "Waveforms", 
                               H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

  return group_id;
}

herr_t ASDF_close_group(hid_t group_id) {
  CHK_H5(H5Gclose(group_id));
  
  return 0;
}

herr_t ASDF_close_dataset(hid_t dataset_id) {
  CHK_H5(H5Dclose(dataset_id));
  
  return 0;
}

hid_t ASDF_create_stations_group(hid_t loc_id, const char *station_name,
                                 const char *station_xml) {
  hid_t group_id, space_id, dcpl, data_id;
  /* Create the group "/Waveform/<station_name>" */
  CHK_H5(group_id = H5Gcreate(loc_id, station_name, 
                              H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
  /* Get some space for the StationXML dataset */
  hsize_t dims[1] = {strlen(station_xml)+1};
  CHK_H5(space_id= H5Screate_simple(1, dims, NULL));

  CHK_H5(dcpl = H5Pcreate(H5P_DATASET_CREATE));

  CHK_H5(data_id = H5Dcreate(group_id, "StationXML", H5T_IEEE_F32LE, space_id,
                             H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

  /* We can write it there for now, 
   * since there is only one stationXML per station */
  CHK_H5(H5Dwrite(data_id, H5T_STD_I8LE, H5S_ALL, H5S_ALL, 
                  H5P_DEFAULT, station_xml));

  CHK_H5(H5Dclose(data_id));
  CHK_H5(H5Pclose(dcpl));

  return group_id;
}

hid_t ASDF_define_waveform(hid_t loc_id, int nsamples, 
                           int start_time, double sampling_rate,
                           const char *event_name, const char *waveform_name) {
  int data_id;
  char char_sampling_rate[10];
  char char_start_time[10];
  
  // converts to decimal base.
  snprintf(char_start_time, sizeof(char_start_time), "%d", start_time);
  snprintf(char_sampling_rate, 
           sizeof(char_sampling_rate), "%1.7f", sampling_rate);

  char char_buf[256];

  hid_t space_id, dcpl;
  hsize_t dims[1] = {nsamples}; // Length of waveform
  CHK_H5(space_id= H5Screate_simple(1, dims, NULL));
  CHK_H5(dcpl = H5Pcreate(H5P_DATASET_CREATE));

  CHK_H5(data_id = H5Dcreate(loc_id, waveform_name, H5T_IEEE_F32LE, space_id,
                                H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
  CHK_H5(ASDF_write_string_attribute(data_id, "starttime",
                                     char_start_time));

  CHK_H5(H5Pclose(dcpl));
  CHK_H5(H5Sclose(space_id));

  return data_id; // Success
}

herr_t ASDF_define_waveforms(hid_t loc_id, int num_waveforms, int nsamples, 
                            int start_time, double sampling_rate,
                            char *event_name, char **waveform_names,
                            int *data_id) {
  int i;
  char char_sampling_rate[10];
  char char_start_time[10];
  
  // converts to decimal base.
  snprintf(char_start_time, sizeof(char_start_time), "%d", start_time);
  snprintf(char_sampling_rate, 
           sizeof(char_sampling_rate), "%1.7f", sampling_rate);

  for (i = 0; i < num_waveforms; ++i) {
    char char_buf[256];

    //sprintf(char_sampling_rate,"%d", sampling_rate); // converts to decimal base

    //CHK_H5(groups[i] = H5Gcreate(loc_id, waveform_names[i],
    //                      H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

    hid_t space_id, dcpl;
    hsize_t dims[1] = {nsamples}; // Length of waveform
    CHK_H5(space_id= H5Screate_simple(1, dims, NULL));
    CHK_H5(dcpl = H5Pcreate(H5P_DATASET_CREATE));

    CHK_H5(data_id[i] = H5Dcreate(loc_id, waveform_names[i], H5T_IEEE_F32LE, space_id,
                                  H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    /*
    CHK_H5(data_id[i] = H5Dcreate(loc_id, waveform_names[i], 
                                  H5T_IEEE_F32LE, space_id,
                                  H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    */

    /*
    CHK_H5(ASDF_write_string_attribute(data_id[i], "event_id",
                                       event_name));
    CHK_H5(ASDF_write_string_attribute(data_id[i], "sampling_rate",
                                       char_sampling_rate));
                                       */
    CHK_H5(ASDF_write_string_attribute(data_id[i], "starttime",
                                       char_start_time));

    CHK_H5(H5Pclose(dcpl));
    CHK_H5(H5Sclose(space_id));
  }
  return 0; // Success
}

herr_t ASDF_write_full_waveform(hid_t data_id, const float *waveform) {
  CHK_H5(H5Dwrite(data_id, H5T_IEEE_F32LE, H5S_ALL, H5S_ALL, 
                  H5P_DEFAULT, waveform));

  return 0; // Success
}

herr_t ASDF_write_partial_waveform(hid_t data_id, const float *waveform, 
                                   int offset, int nsamples) {
  hid_t space_id, slab_id;

  CHK_H5(space_id = H5Dget_space(data_id));

  hsize_t start[1] = {offset};
  hsize_t count[1] = {1};
  hsize_t block[1] = {nsamples};
  CHK_H5(H5Sselect_hyperslab(space_id, H5S_SELECT_SET, start, 
                             NULL, count, block));
  CHK_H5(slab_id = H5Screate_simple(1, block, NULL));
  CHK_H5(H5Dwrite(data_id, H5T_IEEE_F32LE, slab_id, space_id, 
                  H5P_DEFAULT, waveform));
  CHK_H5(H5Sclose(slab_id));
  CHK_H5(H5Sclose(space_id));

  return 0; // Success
}
