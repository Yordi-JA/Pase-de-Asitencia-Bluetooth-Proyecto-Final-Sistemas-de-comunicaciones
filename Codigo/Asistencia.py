import csv
import os
from datetime import datetime

MASTER_FILE_NAME = "ALUMNOS.TXT"

# Function to get the month name in Spanish
def get_month_name(month_number):
    months = {
        1: "Enero", 2: "Febrero", 3: "Marzo", 4: "Abril",
        5: "Mayo", 6: "Junio", 7: "Julio", 8: "Agosto",
        9: "Septiembre", 10: "Octubre", 11: "Noviembre", 12: "Diciembre"
    }
    return months.get(month_number, "Unknown_Month")

def generate_full_report(master_file, log_file_path_input, output_file_path):
    
    # 1. Load Master List (Preserves original file order)
    try:
        master_list_ordered = []
        seen_ids = set() 
        
        with open(master_file, 'r') as f:
            for line in f:
                # Cleans and validates ID
                clean_id = line.strip().replace('.', '')
                
                if clean_id and clean_id.isdigit() and clean_id not in seen_ids:
                    master_list_ordered.append(clean_id)
                    seen_ids.add(clean_id)
                    
            master_list = master_list_ordered 
            
    except FileNotFoundError:
        print(f"ERROR: Master File ({master_file}) not found. Exiting.")
        return False 

    # 2. Load Attendance Log into a Dictionary
    attendance_dict = {}
    
    # Check if log file exists before attempting to open
    if not os.path.exists(log_file_path_input):
        return None 

    try:
        with open(log_file_path_input, 'r') as csvfile:
            reader = csv.reader(csvfile)
            for row in reader:
                if len(row) >= 3: 
                    student_id = row[0].strip()
                    date_part = row[1].strip()
                    time_part = row[2].strip()
                    attendance_dict[student_id] = [date_part, time_part] 
    except Exception as e:
        print(f"ERROR: Could not read or parse the attendance log file. {e}")
        return False

    # 3. Generate the Final Ordered List
    try:
        # Overwrite the output file (which is the log file path)
        with open(output_file_path, 'w', newline='') as outfile:
            writer = csv.writer(outfile)
            
            # Write header
            writer.writerow(['No. Cuenta', 'Asistencia', 'Hora'])
            
            for student_id in master_list: 
                if student_id in attendance_dict:
                    # Student is PRESENT
                    date, time_recorded = attendance_dict[student_id] 
                    writer.writerow([student_id, 1, time_recorded])
                else:
                    # Student is ABSENT
                    writer.writerow([student_id, 0, 0]) 
    
        return True 
        
    except Exception as e:
        print(f"ERROR while writing the report file: {e}")
        return False

if __name__ == "__main__":
    
    successful_run = False
    
    while not successful_run:
        try:
            # 1. Get date
            base_filename = input("Date of the attendance log (YYYYMMDD): ")
            
            # 2. Construct file names
            log_filename_input = f"{base_filename}.CSV" 

            # 3. Logic to create the monthly folder and determine the output path
            if not base_filename.isdigit() or len(base_filename) != 8:
                 raise ValueError("Format must be 8 digits (YYYYMMDD)")

            date_obj = datetime.strptime(base_filename, "%Y%m%d")
            month_num = date_obj.month
            year_str = date_obj.strftime("%Y")
            month_name = get_month_name(month_num)
            
            folder_name = f"{year_str}-{month_num} ({month_name})"
            folder_path = os.path.join("Asistencias", folder_name)
            
            os.makedirs(folder_path, exist_ok=True)
            
            # The output file (which overwrites the log) goes inside the monthly folder
            full_output_path = os.path.join(folder_path, log_filename_input)

            # 4. Call the main function
            result = generate_full_report(MASTER_FILE_NAME, log_filename_input, full_output_path)
            
            if result is True:
                successful_run = True
            elif result is None:
                # If the file was not found, print error and loop back to input
                print(f"ERROR: Attendance log file '{log_filename_input}' not found. Please re-enter.")
            else:
                # General error, but file was found
                 print("A general error occurred during report generation. Please check the log contents.")

        except ValueError as ve:
            print(f"INPUT ERROR: {ve}. Please try again.")
        except Exception as e:
            # Catch file system errors or other unexpected failures
            print(f"ERROR: An unexpected error occurred: {e}")
            break 